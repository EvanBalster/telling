#pragma once

#include <optional>

#include "service_reply.h"
#include "service_pull.h"
#include "service_publish.h"

#include "client_request.h"


namespace telling
{
	/*
		Used to enlist services with a Server in the same process.
	*/
	class Enlistment
	{
	public:
		enum STATUS
		{
			INITIAL   = 0,
			REQUESTED = 1,
			ENLISTED  = 2,
			FAILED   = -1,
		};


	public:
		Enlistment(
			std::string_view serverID,
			std::string_view serviceURI);
		Enlistment(
			std::string_view serverID,
			std::string_view serviceURI,
			std::string_view serviceURI_enlist_as);
		~Enlistment();

		/*
			Check enlistment status.
		*/
		STATUS                status()     const noexcept;
		const nng::exception &exception()  const noexcept;
		bool                  isWorking () const noexcept    {auto s=status(); return s==INITIAL || s==REQUESTED;}
		bool                  isEnlisted() const noexcept    {return status() == ENLISTED;}


	public:
		class Delegate;
		std::shared_ptr<AsyncQuery> delegate;
		client::Request_Async       requester;
	};


	/*
		Base class for Services.
	*/
	class Service_Base
	{
	public:
		// In-process URI.
		const std::string         uri;
		HostAddress::Base         inProcAddress() const noexcept    {return HostAddress::Base::InProc(uri);}

		// Primary enlistment.  Additional enlistments under different URIs are allowed.
		std::optional<Enlistment> enlistment;


	public:
		Service_Base(std::string _uri, std::string_view serverID = std::string_view());
		virtual ~Service_Base();

		void enlist(std::string_view serverID);


		/*
			Services can use serverID instead of manually listening.
				It is unusual but possible for a service to dial a listening client.
				When a client is connected directly to a service, no URI routing occurs.
		*/
		void dial         (const HostAddress::Base &base)             {Each_Dial      (base, _replier(), _publisher(), _puller());}
		void listen       (const HostAddress::Base &base)             {Each_Listen    (base, _replier(), _publisher(), _puller());}
		void disconnect   (const HostAddress::Base &base) noexcept    {Each_Disconnect(base, _replier(), _publisher(), _puller());}
		void disconnectAll()                              noexcept    {Each_DisconnectAll   (_replier(), _publisher(), _puller());}
		void close        ()                              noexcept    {Each_Close           (_replier(), _publisher(), _puller());}


	protected:
		virtual service::Reply_Base   &_replier()   noexcept = 0;
		virtual service::Publish_Base &_publisher() noexcept = 0;
		virtual service::Pull_Base    &_puller()    noexcept = 0;
	};


	/*
		A non-blocking service which is checked like a mailbox.
	*/
	class Service_Box : public Service_Base
	{
	protected:
		service::Reply_Box   replier;
		service::Publish_Box publisher;
		service::Pull_Box    puller;

		service::Reply_Base   &_replier()   noexcept final    {return replier;}
		service::Publish_Base &_publisher() noexcept final    {return publisher;}
		service::Pull_Base    &_puller()    noexcept final    {return puller;}


	public:
		Service_Box(std::string _uri, std::string_view serverID = std::string_view());
		~Service_Box() override;

		void enlist(std::string_view serverID);


		/*
			Publish a message to a topic (URI).
		*/
		void publish(nng::msg &&msg)         {publisher.publish(std::move(msg));}


		/*
			Receive pushed messages.
		*/
		bool pull(nng::msg &msg)             {return puller.pull(msg);}


		/*
			Receive and reply to requests (one by one).
		*/
		bool receive(nng::msg  &request)     {return replier.receive(request);}
		void respond(nng::msg &&reply)       {replier.respond(std::move(reply));}

		/*
			Reply to all pending requests with a functor.
		*/
		template<class Fn>          void respond_all(Fn fn)           {replier.respond_all(fn);}
		template<class Fn, class A> void respond_all(Fn fn, A arg)    {replier.respond_all(fn, arg);}
	};


	/*
		A service which receives and responds to messages using asynchronous events.
	*/
	class Service_Async : public Service_Base
	{
	public:
		using SendDirective = AsyncOp::SendDirective;
		using Directive     = AsyncOp::Directive;

		/*
			Asynchronous events are delivered to a delegate object.
		*/
		class Handler
		{
		public:
			virtual ~Handler() {}


			/*
				Receive push messages and requests via one method.
					Incoming messages should be formatted as MsgView::Request.
					Push messages will have a QueryID of 0 and don't support replying.
					Requests have non-zero QueryID and support immediate or delayed replies.

				Alternatively, override pull_recv and request_recv separately.
			*/
			virtual SendDirective recv(QueryID id, nng::msg &&msg) = 0;


			// Receive a pull message.
			// There is no method for replying.
			virtual Directive     pull_recv (nng::msg &&request) {auto d=recv(0, std::move(request)); return d.directive;}
			virtual Directive     pull_error(nng::error)         {}

			// Receive a request.
			// May respond immediately (return a msg) or later (via respondTo).
			virtual SendDirective request_recv (QueryID id, nng::msg &&request) {return recv(id, std::move(request));}

			// Reply processing status (optional).
			// reply_error may be triggered
			virtual void          reply_sent (QueryID id)             {}
			virtual Directive     reply_error(QueryID id, nng::error) {return AsyncOp::TERMINATE;}

			// Publish outbox status (optional)
			virtual SendDirective publish_sent ()           {}
			virtual SendDirective publish_error(nng::error) {return AsyncOp::TERMINATE;}

			// Pipe events.
			//virtual void pipeEvent(Communicator&, nng::pipe_view, nng::pipe_ev) {}
		};

		class Delegate;


	protected:
		std::shared_ptr<Delegate> delegate;
		service::Reply_Async      replier;
		service::Pull_Async       puller;
		service::Publish_Async    publisher;

		service::Reply_Base   &_replier()   noexcept final    {return replier;}
		service::Publish_Base &_publisher() noexcept final    {return publisher;}
		service::Pull_Base    &_puller()    noexcept final    {return puller;}


	public:
		Service_Async(std::shared_ptr<Handler> handler,
			std::string _uri, std::string_view serverID = std::string_view());
		~Service_Async();

		/*
			Publish a message to a topic (URI).
		*/
		void publish(nng::msg &&bulletin)                       {publisher.publish(std::move(bulletin));}

		/*
			Respond to the query with the given ID.
		*/
		void respondTo(QueryID queryID, nng::msg &&reply)       {replier.respondTo(queryID, std::move(reply));}
	};
}