#pragma once


#include <utility>
#include <memory>
#include <thread>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <map>
#include <robin_hood.h>
#include <nngpp/nngpp.h>
#include <tsl/htrie_map.h>

#include "host_address.h"
#include "io_queue.h"
#include "msg_view.h"

#include "socket.h"

#include "service_pull.h"
#include "client_push.h"
#include "service_publish.h"
#include "client_subscribe.h"
#include "service_reply.h"


namespace telling
{
	template<typename value_type>
	using Dictionary = robin_hood::unordered_flat_map<std::string, value_type>;

	template<typename value_type>
	using PrefixMap = tsl::htrie_map<char, value_type>;


	class Server : public UsingPatternEnums
	{
	public:
		Server(std::string ID, std::ostream *log = nullptr);
		~Server();

		// TODO configure, start and stop endpoints
		/*void start_tcp(uint16_t base_port, PATTERNS patterns = ALL);
		void stop_tcp (uint16_t base_port, PATTERNS patterns = ALL);*/

		void open (const HostAddress::Base &);
		void close(const HostAddress::Base &);




	protected:
		static void run_req_rep  () noexcept;
		static void run_pub_sub  () noexcept;
		static void run_push_pull() noexcept;


	public:
		const std::string ID;

		const HostAddress::Base
			address_services, // Service dial-in
			address_internal; // Server components acting like clients


	private:
		std::ostream &log;

		/*
			...
		*/
		struct Device_AIO
		{
			nng::aio aio;
		};


		// Generic delegate for received messages.
		template<typename Module, typename MsgViewType>
		class DelegateRecv : public AsyncRecv
		{
		public:
			DelegateRecv(Module *_module) : module(_module) {}
			~DelegateRecv() {}
			Directive asyncRecv_msg(nng::msg &&msg) override; // Below
			Directive asyncRecv_error(nng::error error) override; // Below

			void stop() {std::lock_guard<std::mutex> g(mtx); module = nullptr;}

		public:
			std::mutex mtx;
			Module    *module;
		};


		class Route;
		class Services;


		/*
			Published messages are routed to all subscribers.
				Services dial into sub_internal and publish their paths.
		*/
		class Publish
		{
		public:
			Publish();
			~Publish();

			Server *server();
			static const char *Name()    {return "*PUB";}
			Socket &hostSocket()         {return *publish.socket();}

		protected:
			friend class Services; // for now

			using Delegate_Sub = DelegateRecv<Publish, MsgView::Bulletin>;
			std::shared_ptr<Delegate_Sub> sub_delegate;

			client::Subscribe_Async subscribe;
			service::Publish_Box    publish;

			friend class Delegate_Sub;
			AsyncOp::Directive received(const MsgView::Bulletin&, nng::msg&&);
			AsyncOp::Directive receive_error(Delegate_Sub*, nng::error);
		}
			publish;
		
		/*
			Push-pull pattern.  Messages are routed by path.
		*/
		class Pull
		{
		public:
			Pull();
			~Pull();

			Server *server();
			static const char *Name()    {return "*PULL";}
			Socket &hostSocket()         {return *pull.socket();}

		protected:
			using Delegate_Pull = DelegateRecv<Pull, MsgView::Request>;
			std::shared_ptr<Delegate_Pull> pull_delegate;

			service::Pull_Async pull;

			friend class Delegate_Pull;
			AsyncOp::Directive received(const MsgView::Request&, nng::msg&&);
			AsyncOp::Directive receive_error(Delegate_Pull*, nng::error);
		}
			pull;

		/*
			Requests are routed to services by prefix.
			Replies are routed to requesters with a backtrace.
		*/
		class Reply
		{
		public:
			using Delegate_Reply   = DelegateRecv<Reply, MsgView::Reply>;
			using Delegate_Request = DelegateRecv<Reply, MsgView::Request>;


		public:
			Reply();
			~Reply();

			Server *server();
			static const char *Name()    {return "*REP";}
			Socket &hostSocket()         {return reply_ext;}


			std::shared_ptr<Delegate_Reply> reply_handler()    {return delegate_reply;}


		protected:
			std::shared_ptr<Delegate_Reply>   delegate_reply;
			std::shared_ptr<Delegate_Request> delegate_request;

			Socket
				reply_ext,   // <--> clients
				request_dvc, // connects int and ext with a device
				reply_int;   // <--> services

			// I/O handling for replies
			std::shared_ptr<AsyncSendQueue>       rep_sendQueue;
			AsyncSend::Operator<nng::socket_view> rep_send;
			AsyncRecv::Operator<nng::socket_view> rep_recv;

			friend class Delegate_Reply;
			friend class Delegate_Request;
			AsyncOp::Directive received(const MsgView::Reply&,   nng::msg&&);
			AsyncOp::Directive received(const MsgView::Request&, nng::msg&&);

			AsyncOp::Directive receive_error(Delegate_Request*, nng::error);
			AsyncOp::Directive receive_error(Delegate_Reply  *, nng::error);


		private:
			std::thread thread_device;
			static void run_device(Reply*);
		}
			reply;



		/*
			Connection to a Service.
				Services class manages the routing table.
		*/
		class Route
		{
		public:
			Server            &server;
			const std::string  path;


			void sendPush   (nng::msg &&msg);
			void sendRequest(nng::msg &&msg);
			

		public:
			Route(Server &server, std::string path);
			~Route();

			void dial(const HostAddress::Base&);


		protected:
			class RequestRaw : public Socket
			{
			public:
				RequestRaw(Route &_route) : Socket(Role::CLIENT, Pattern::REQ_REP, Socket::RAW), route(_route) {}
				~RequestRaw() {}

			protected:
				Route &route;
			};

			RequestRaw       req;
			client::Push_Box push;

			// I/O handling for requests
			std::shared_ptr<AsyncSendQueue>       req_sendQueue;
			AsyncSend::Operator<nng::socket_view> req_send;
			AsyncRecv::Operator<nng::socket_view> req_recv;

			std::mutex mtx;
			bool halted = false;
		};


		/*
			Directory of services.
		*/
		class Services
		{
		public:
			/*
				Route messages to an appropriate service, returning a status:
					200 -- the message was routed
					404 -- no service matches the path
					503 -- failed to send
			*/

			Status routePush(std::string_view path, nng::msg &&msg)
			{
				std::lock_guard<std::mutex> g(mtx);

				auto r = route(path);
				if (!r) return StatusCode::NotFound;
				try
				{
					r->sendPush(std::move(msg));
				}
				catch (nng::exception e)
				{
					// TODO account for issues?
					return StatusCode::ServiceUnavailable;
				}
				return StatusCode::OK;
			}

			Status routeRequest(std::string_view path, nng::msg &&msg)
			{
				std::lock_guard<std::mutex> g(mtx);

				auto r = route(path);
				if (!r) return StatusCode::NotFound;
				try
				{
					r->sendRequest(std::move(msg));
				}
				catch (nng::exception e)
				{
					// TODO account for issues?
					return StatusCode::ServiceUnavailable;
				}
				return StatusCode::OK;
			}


		protected:
			using Delegate_Sub = DelegateRecv<Services, MsgView::Bulletin>;

			using PipeID = decltype(std::declval<nng_pipe>().id);

			std::mutex         mtx;
			PrefixMap<Route*>  map;

			struct NewRoute
			{
				QueryID           queryID;
				PipeID            pipeID;
				std::string       map_uri;
				HostAddress::Base host;
			};

			struct Management
			{
				std::deque<NewRoute>    route_open;
				std::deque<Route*>      route_close;
				std::thread             thread;
				std::condition_variable cond;
				bool                    run = true;
			}
				management;

			void run_management_thread();

			class EnlistResponder;
			friend class EnlistResponder;
			std::shared_ptr<AsyncRespond> enlist_responder;
			service::Reply_Async          enlist_reply;

			std::unordered_map<PipeID, std::string> enlistmentMap;

			AsyncOp::SendDirective enlistRequest(QueryID, nng::msg &&);
			void                   enlistExpired(nng::pipe_view);

			service::Publish_Box publish_events;
			

			Route *route(std::string_view path)
			{
				auto pos = map.longest_prefix(path);
				return (pos == map.end()) ? nullptr : &**pos;
			}


		public:
			Services();
			~Services();

			Server *server();
			static const char *Name()    {return "*services";}
		}
			services;
	};


	#define TELLING_SERVER_FROM_MODULE(self, member) \
		reinterpret_cast<telling::Server*>(intptr_t(self) - \
			ptrdiff_t(offsetof(telling::Server, member)))

	inline Server *Server::Services::server() {return TELLING_SERVER_FROM_MODULE(this, services);}
	inline Server *Server::Reply   ::server() {return TELLING_SERVER_FROM_MODULE(this, reply);}
	inline Server *Server::Publish ::server() {return TELLING_SERVER_FROM_MODULE(this, publish);}
	inline Server *Server::Pull    ::server() {return TELLING_SERVER_FROM_MODULE(this, pull);}

	#undef TELLING_SERVER_FROM_MODULE


	template<typename Module, typename MsgViewType>
	AsyncOp::Directive Server::DelegateRecv<Module, MsgViewType>::asyncRecv_msg(nng::msg &&_msg)
	{
		// Lock out other receives and stop().
		std::lock_guard<std::mutex> g(mtx);

		if (!module)
		{
			return AsyncOp::TERMINATE;
		}

		// Retain message.
		nng::msg msg = std::move(_msg);

		auto server = module->server();
		auto &log = server->log;

		try
		{
			MsgViewType parsedMsg = msg;
			auto result = module->received(parsedMsg, std::move(msg));
			return result;
		}
		catch (MsgException e)
		{
			log << Module::Name() << ": message exception: " << e.what() << std::endl;
		}

		return AsyncOp::DECLINE;
	}

	template<typename Module, typename MsgViewType>
	AsyncOp::Directive Server::DelegateRecv<Module, MsgViewType>::asyncRecv_error(nng::error error)
	{
		// Lock out other receives and stop().
		std::lock_guard<std::mutex> g(mtx);

		if (!module) return AsyncOp::TERMINATE;

		return module->receive_error(this, error);
	}
}