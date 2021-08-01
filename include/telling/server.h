#pragma once


#include <utility>
#include <memory>
#include <thread>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <map>
#include <nngpp/nngpp.h>
#include <tsl/htrie_map.h>
#include <life_lock.h>

#include "host_address.h"
#include "io_queue.h"
#include "msg_view.h"

#include "socket.h"

#include "service_pull.h"
#include "service_publish.h"
#include "service_reply.h"
#include "service.h"

#include "client_push.h"
#include "client_subscribe.h"
#include "service_registration.h"


namespace telling
{
	template<typename value_type>
	using PrefixMap = tsl::htrie_map<char, value_type>;


	/*
		Telling server.
			Handles URI routing to services as they register and vanish.
			Propagates published messages to clients.
			Allows for multiple methods of connectivity to an inproc network.
	*/
	class Server : public UsingPatternEnums
	{
	public:
		/*
			Create a server.
				log -- optional logging stream
				ID  -- in-proc hostname, also used for registration and internals
				open_inproc -- if true, immediately open server to inproc clients.
		*/
		Server(
			std::ostream    *log         = nullptr,
			std::string_view ID          = DefaultServerID(),
			bool             open_inproc = true);
		~Server();

		// TODO configure, start and stop endpoints
		/*void start_tcp(uint16_t base_port, PATTERNS patterns = ALL);
		void stop_tcp (uint16_t base_port, PATTERNS patterns = ALL);*/

		void open (const HostAddress::Base &);
		void close(const HostAddress::Base &);


	public:
		// In-process ID
		const std::string ID;

		const HostAddress::Base
			address_register, // Service dial-in
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


		class Route;
		class Services;


		/*
			Published messages are routed to all subscribers.
				Services dial into sub_internal and publish their paths.
		*/
		class PubSub : public AsyncRecv<Subscribing>
		{
		public:
			PubSub(Server&);
			~PubSub();

			std::weak_ptr<PubSub> get_weak()    {return async_lifetime.get_weak(this);}

			static const char *Name()    {return "*PUB";}
			Socket &hostSocket()         {return *publish.socket();}

		protected:
			friend class Services; // for now
			
			Server &server;

			Subscribe   subscribe;
			Publish_Box publish;

			void async_recv (Subscribing, nng::msg&&) override;
			void async_error(Subscribing, AsyncError) override;

			edb::life_lock_self async_lifetime;
		}
			publish;
		
		/*
			Push-pull pattern.  Messages are routed by path.
		*/
		class PushPull : public AsyncRecv<Pulling>
		{
		public:
			PushPull(Server&);
			~PushPull();

			std::weak_ptr<PushPull> get_weak()    {return async_lifetime.get_weak(this);}

			static const char *Name()    {return "*PULL";}
			Socket &hostSocket()         {return *pull.socket();}

		protected:
			Server &server;
			
			Pull pull;

			void async_recv (Pulling, nng::msg&&) override;
			void async_error(Pulling, AsyncError) override;

			edb::life_lock_self async_lifetime;
		}
			pull;

		/*
			Requests are routed to services by prefix.
			Replies are routed to requesters with a backtrace.
		*/
		class ReqRep;
		class ClientRequesting : public TagSend<void> {};
		class ServerResponding : public TagSend<void> {};
		class ServiceReplying {};

		class ReqRep :
			public AsyncRecv<ClientRequesting>,
			public AsyncRecv<ServiceReplying>
		{
		public:
			ReqRep(Server&);
			~ReqRep();

			std::weak_ptr<ReqRep> get_weak()    {return async_lifetime.get_weak(this);}

			static const char *Name()    {return "*REP";}
			Socket &hostSocket()         {return reply_ext;}


		protected:
			Server &server;
			
			Socket
				reply_ext,   // <--> clients
				request_dvc, // connects int and ext with a device
				reply_int;   // <--> services

			// I/O handling for replies
			edb::life_locked<AsyncSendQueue<ServerResponding>> rep_sendQueue;
			AsyncSendLoop   <ServerResponding>                 rep_send;
			AsyncRecvLoop   <ClientRequesting>                 rep_recv;

			void async_recv (ClientRequesting, nng::msg&&) override;
			void async_error(ClientRequesting, AsyncError) override;
			void async_recv (ServiceReplying,  nng::msg&&) override;
			void async_error(ServiceReplying,  AsyncError) override;


		private:
			std::thread thread_device;
			static void run_device(ReqRep*);

		protected:
			edb::life_lock_self async_lifetime;
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

			RequestRaw req;
			Push_Box   push;

			// I/O handling for requests
			edb::life_locked<AsyncSendQueue<ClientRequesting>> req_sendQueue;
			AsyncSendLoop<ClientRequesting>                    req_send_to_service;
			AsyncRecvLoop<ServiceReplying>                     req_recv_from_service;

			std::mutex mtx;
			bool halted = false;
		};


		/*
			Directory of services.
		*/
		class Services :
			public AsyncReply,
			public Socket::PipeEventHandler
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

			Reply register_reply;

			std::unordered_map<PipeID, std::string> registrationMap;


			// Handlers
			void pipeEvent(Socket *socket, nng::pipe_view pipe, nng::pipe_ev event) final;
			void async_recv (Replying rep, nng::msg &&query)  final;
			void async_sent (Replying rep) final;
			void async_error(Replying rep, AsyncError status) final;

			Publish_Box publish_events;
			

			Route *route(std::string_view path)
			{
				auto pos = map.longest_prefix(path);
				return (pos == map.end()) ? nullptr : &**pos;
			}


		public:
			Services(Server&);
			~Services();

			std::weak_ptr<Services> get_weak()    {return async_lifetime.get_weak(this);}

			static const char *Name()    {return "*services";}

		protected:
			Server &server;
		
			edb::life_lock_self async_lifetime;
		}
			services;
	};
}
