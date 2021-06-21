#pragma once

#include <optional>

#include "service_base.h"
#include "msg.h"


namespace telling
{
	/*
		A non-blocking service which is checked like a mailbox.
	*/
	class Service_Box : public Service_Base
	{
	public:
		Service_Box(std::string _uri, std::string_view serverID = DefaultServerID());
		~Service_Box() override;


		/*
			Publish a message to a topic (URI).
		*/
		void publish(nng::msg &&msg) final   {_publisher.publish(std::move(msg));}


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
		Reply_Base   *replier()   noexcept final    {return &_replier;}
		Publish_Base *publisher() noexcept final    {return &_publisher;}
		Pull_Base    *puller()    noexcept final    {return &_puller;}


	protected:
		Reply_Box   _replier;
		Publish_Box _publisher;
		Pull_Box    _puller;
	};


	/*
		This service handler parses incoming Requests and separates them by Method.
	*/
	class Reactor : public ServiceHandler
	{
	public:
		virtual ~Reactor() {}


	protected:
		struct Query
		{
			QueryID    id;
			SendPrompt reply;

			// Call this to signal the Reactor will reply later through Service API.
			void defer() noexcept    {}
		};

		// Return value when Reactor intends to defer reply.
		nng::msg NotImplemented(UriView);


		/*
			Services must always implement GET requests (but may decline them).
				The semantics of these methods are as defined in HTTP.
				The CONNECT method is not supported.

			Requests have non-zero QueryID and support immediate or delayed replies.
				Reply immediately by returning an nng::msg&& from the function.
				Reply later by using the QueryID with calls to Service.

			Push messages will have a QueryID of 0 and don't support replying.
				In this case, returned messages will be discarded.
				Push/Pull messages that don't satisfy Method::allowNoResponse will be ignored.
		*/

		// Return the set of allowed methods.
		virtual Methods  allowed(UriView uri) const noexcept = 0;

		// Safe methods (no PUSH support)
		virtual void async_get    (Query q, Msg::Request &&req) = 0;
		virtual void async_head   (Query q, Msg::Request &&req)    {q.reply(NotImplemented(req.uri()));}
		virtual void async_trace  (Query q, Msg::Request &&req);
		virtual void async_options(Query q, Msg::Request &&req);

		// Idempotent methods
		virtual void async_put    (Query q, Msg::Request &&req)    {q.reply(NotImplemented(req.uri()));}
		virtual void async_delete (Query q, Msg::Request &&req)    {q.reply(NotImplemented(req.uri()));}

		// Other methods
		virtual void async_patch  (Query q, Msg::Request &&req)    {q.reply(NotImplemented(req.uri()));}
		virtual void async_post   (Query q, Msg::Request &&req)    {q.reply(NotImplemented(req.uri()));}

		// Undefined (as of HTTP/1.1) method names
		virtual void async_UNKNOWN(Query q, Msg::Request &&req)    {q.reply(NotImplemented(req.uri()));}


	protected:
		// Implementation...
		void _handle(Query, nng::msg &&);
		void async_recv(Pulling,      nng::msg &&request) override    {_handle(Query{0},                std::move(request));}
		void async_recv(Replying rep, nng::msg &&request) override    {_handle(Query{rep.id, rep.send}, std::move(request));}

		std::mutex    reactor_mtx;
	};


	/*
		A service which receives and responds to messages using asynchronous events.
	*/
	class Service : public Service_Base
	{
	public:
		Service(std::weak_ptr<ServiceHandler_Base> handler,
			std::string _uri, std::string_view serverID = DefaultServerID());
		~Service();

		/*
			Publish a message to a topic (URI).
		*/
		void publish(nng::msg &&bulletin) final                 {_publisher.publish(std::move(bulletin));}

		/*
			Respond to the query with the given ID.
		*/
		void respondTo(QueryID queryID, nng::msg &&reply)       {_replier.respondTo(queryID, std::move(reply));}


		// Access communicators.
		Reply_Base   *replier()   noexcept final    {return &_replier;}
		Publish_Base *publisher() noexcept final    {return &_publisher;}
		Pull_Base    *puller()    noexcept final    {return &_puller;}


	protected:
		//std::weak_ptr<Handler> handler;
		Reply     _replier;
		Pull      _puller;
		Publish   _publisher;
	};
}