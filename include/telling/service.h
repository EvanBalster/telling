#pragma once

#include <optional>

#include "service_base.h"
#include "msg_view.h"


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
		service::Reply_Base   *replier()   noexcept final    {return &_replier;}
		service::Publish_Base *publisher() noexcept final    {return &_publisher;}
		service::Pull_Base    *puller()    noexcept final    {return &_puller;}


	protected:
		service::Reply_Box   _replier;
		service::Publish_Box _publisher;
		service::Pull_Box    _puller;
	};


	/*
		This service handler parses incoming Requests and separates them by Method.
	*/
	class Reactor : public ServiceHandler
	{
	public:
		virtual ~Reactor() {}


	protected:
		/*
			Services must always implement GET requests (but may decline them).
				The semantics of these methods are as defined in HTTP.
				The CONNECT method is not supported.

			Requests have non-zero QueryID and support immediate or delayed replies.
				Reply immediately by returning an nng::msg&& from the function.
				Reply later by using the QueryID with calls to Service_Async,

			Push messages will have a QueryID of 0 and don't support replying.
				Push/Pull messages that don't satisfy Method::allowNoResponse will be ignored.
		*/

		// Return the set of allowed methods.
		virtual Methods       allowed() const noexcept = 0;

		// Safe methods (no PUSH support)
		virtual SendDirective recv_get    (QueryID id, const MsgView::Request &req, nng::msg &&msg) = 0;
		virtual SendDirective recv_head   (QueryID id, const MsgView::Request &req, nng::msg &&msg)    {return DECLINE;}
		virtual SendDirective recv_trace  (QueryID id, const MsgView::Request &req, nng::msg &&msg)    {return DECLINE;}
		virtual SendDirective recv_options(QueryID id, const MsgView::Request &req, nng::msg &&msg);

		// Idempotent methods
		virtual SendDirective recv_put    (QueryID id, const MsgView::Request &req, nng::msg &&msg)    {return DECLINE;}
		virtual SendDirective recv_delete (QueryID id, const MsgView::Request &req, nng::msg &&msg)    {return DECLINE;}

		// Other methods
		virtual SendDirective recv_patch  (QueryID id, const MsgView::Request &req, nng::msg &&msg)    {return DECLINE;}
		virtual SendDirective recv_post   (QueryID id, const MsgView::Request &req, nng::msg &&msg)    {return DECLINE;}

		// Undefined (as of HTTP/1.1) method names
		virtual SendDirective recv_UNKNOWN(QueryID id, const MsgView::Request &req, nng::msg &&msg)    {return DECLINE;}


	protected:
		// Implementation...
		SendDirective _handle     (QueryID,    nng::msg &&);
		Directive     pull_recv   (            nng::msg &&request) override;
		SendDirective request_recv(QueryID id, nng::msg &&request) override;

		std::mutex    reactor_mtx;
	};


	/*
		A service which receives and responds to messages using asynchronous events.
	*/
	class Service_Async : public Service_Base
	{
	public:
		Service_Async(std::weak_ptr<ServiceHandler_Base> handler,
			std::string _uri, std::string_view serverID = DefaultServerID());
		~Service_Async();

		/*
			Publish a message to a topic (URI).
		*/
		void publish(nng::msg &&bulletin) final                 {_publisher.publish(std::move(bulletin));}

		/*
			Respond to the query with the given ID.
		*/
		void respondTo(QueryID queryID, nng::msg &&reply)       {_replier.respondTo(queryID, std::move(reply));}


		// Access communicators.
		service::Reply_Base   *replier()   noexcept final    {return &_replier;}
		service::Publish_Base *publisher() noexcept final    {return &_publisher;}
		service::Pull_Base    *puller()    noexcept final    {return &_puller;}


	protected:
		//std::weak_ptr<Handler> handler;
		service::Reply_Async     _replier;
		service::Pull_Async      _puller;
		service::Publish_Async   _publisher;
	};
}