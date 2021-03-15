#pragma once

#include <optional>

#include "service_registration.h"

#include "service_reply.h"
#include "service_pull.h"
#include "service_publish.h"

#include "client_request.h"


namespace telling
{
	/*
		Base class for Services.
	*/
	class Service_Base
	{
	public:
		// In-process URI.
		const std::string         uri;
		HostAddress::Base         inProcAddress() const noexcept    {return HostAddress::Base::InProc(uri);}

		// Primary registration.  Additional registrations under different URIs are allowed.
		std::optional<Registration> registration;


	public:
		Service_Base(std::string _uri, std::string_view serverID = std::string_view());
		virtual ~Service_Base();

		void registerURI(std::string_view serverID);


		/*
			Services can use serverID instead of manually listening.
				It is unusual but possible for a service to dial a listening client.
				When a client is connected directly to a service, no URI routing occurs.
		*/
		void dial         (const HostAddress::Base &base)             {Dial      (base, replier(), publisher(), puller());}
		void listen       (const HostAddress::Base &base)             {Listen    (base, replier(), publisher(), puller());}
		void disconnect   (const HostAddress::Base &base) noexcept    {Disconnect(base, replier(), publisher(), puller());}
		void disconnectAll()                              noexcept    {DisconnectAll   (replier(), publisher(), puller());}
		void close        ()                              noexcept    {Close           (replier(), publisher(), puller());}


		/*
			Access individual communicators.
		*/
		virtual service::Reply_Base   *replier()   noexcept = 0;
		virtual service::Publish_Base *publisher() noexcept = 0;
		virtual service::Pull_Base    *puller()    noexcept = 0;
	};


	/*
		A non-blocking service which is checked like a mailbox.
	*/
	class Service_Box : public Service_Base
	{
	public:
		Service_Box(std::string _uri, std::string_view serverID = std::string_view());
		~Service_Box() override;


		/*
			Publish a message to a topic (URI).
		*/
		void publish(nng::msg &&msg)         {_publisher.publish(std::move(msg));}


		/*
			Receive pushed messages.
		*/
		bool pull(nng::msg &msg)             {return _puller.pull(msg);}


		/*
			Receive and reply to requests (one by one).
		*/
		bool receive(nng::msg  &request)     {return _replier.receive(request);}
		void respond(nng::msg &&reply)       {_replier.respond(std::move(reply));}

		/*
			Reply to all pending requests with a functor.
		*/
		template<class Fn>          void respond_all(Fn fn)           {_replier.respond_all(fn);}
		template<class Fn, class A> void respond_all(Fn fn, A arg)    {_replier.respond_all(fn, arg);}


		// Access communicators.
		service::Reply_Base   *replier()   noexcept final    {return &_replier;}
		service::Publish_Base *publisher() noexcept final    {return &_publisher;}
		service::Pull_Base    *puller()    noexcept final    {return &_puller;}


	protected:
		service::Reply_Box   _replier;
		service::Publish_Box _publisher;
		service::Pull_Box    _puller;
	};


	/*
		A service which receives and responds to messages using asynchronous events.
	*/
	class Service_Async : public Service_Base
	{
	public:
		/*
			Asynchronous events are delivered to a delegate object.
		*/
		class Handler :
			public AsyncSend,
			public AsyncRecv,
			public AsyncRespond
		{
		public:
			virtual ~Handler() {}

			using SendDirective = AsyncOp::SendDirective;
			using Directive     = AsyncOp::Directive;


		protected:
			// Receive a pull message.
			// There is no method for replying.
			virtual Directive     pull_recv (nng::msg &&request) = 0;
			virtual Directive     pull_error(nng::error)         {}

			// Receive a request.
			// May respond immediately (return a msg) or later (via respondTo).
			virtual SendDirective request_recv (QueryID id, nng::msg &&request) = 0;

			// Reply processing status (optional).
			// reply_error may be also be triggered if there is some error receiving a request.
			virtual void          reply_sent (QueryID id)                {}
			virtual Directive     reply_error(QueryID id, nng::error)    {return AsyncOp::TERMINATE;}

			// Publish outbox status (optional)
			virtual SendDirective publish_sent ()              {}
			virtual SendDirective publish_error(nng::error)    {return AsyncOp::TERMINATE;}

			// Optionally receive pipe events from the various sockets.
			virtual void pipeEvent(Socket*, nng::pipe_view, nng::pipe_ev) {}


		private:
			SendQueueMtx publishQueue;

			// AsyncRespond impl.
			SendDirective asyncRespond_recv (QueryID qid, nng::msg &&m)    final    {return this->request_recv(qid, std::move(m));}
			void          asyncRespond_done (QueryID qid)                  final    {this->reply_sent(qid);}
			Directive     asyncRespond_error(QueryID qid, nng::error e)    final    {return this->reply_error(qid, e);}

			// AsyncRecv (Pull) impl.
			Directive asyncRecv_msg  (nng::msg &&msg   ) final    {return this->pull_recv(std::move(msg));}
			Directive asyncRecv_error(nng::error status) final    {return this->pull_error(status);}

			// AsyncSend (Publish) impl.
			SendDirective asyncSend_msg  (nng::msg &&msg)    final;
			SendDirective asyncSend_sent ()                  final;
			SendDirective asyncSend_error(nng::error status) final    {return this->publish_error(status);}
		};

		/*
			A convenience variant of Handler, allowing for simpler implementations.
		*/
		class Handler_Ex : public Handler
		{
		public:
			virtual ~Handler_Ex() {}


			/*
				Receive push messages and requests via one method.
					Incoming messages should be formatted as MsgView::Request.
					Push messages will have a QueryID of 0 and don't support replying.
					Requests have non-zero QueryID and support immediate or delayed replies.

				Alternatively, override pull_recv and request_recv separately.
			*/
			virtual SendDirective recv(QueryID id, nng::msg &&msg) = 0;


		protected:
			// Implementation...
			Directive     pull_recv   (            nng::msg &&request) override
				{auto d=recv(QueryID(0), std::move(request)); return d.directive;}
			SendDirective request_recv(QueryID id, nng::msg &&request) override
				{return recv(id, std::move(request));}
		};


	public:
		Service_Async(std::shared_ptr<Handler> handler,
			std::string _uri, std::string_view serverID = std::string_view());
		~Service_Async();

		/*
			Publish a message to a topic (URI).
		*/
		void publish(nng::msg &&bulletin)                       {_publisher.publish(std::move(bulletin));}

		/*
			Respond to the query with the given ID.
		*/
		void respondTo(QueryID queryID, nng::msg &&reply)       {_replier.respondTo(queryID, std::move(reply));}


		// Access communicators.
		service::Reply_Base   *replier()   noexcept final    {return &_replier;}
		service::Publish_Base *publisher() noexcept final    {return &_publisher;}
		service::Pull_Base    *puller()    noexcept final    {return &_puller;}


	protected:
		//std::shared_ptr<Handler> handler;
		service::Reply_Async     _replier;
		service::Pull_Async      _puller;
		service::Publish_Async   _publisher;
	};
}