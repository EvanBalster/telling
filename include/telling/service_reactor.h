#pragma once

#include "msg.h"
#include "service_base.h"


namespace telling
{


	/*
		This service handler parses incoming Requests and separates them by Method.
	*/
	class Reactor : public ServiceHandler
	{
	public:
		using UriView    = telling::UriView;
		using Status     = telling::Status;
		using MethodCode = telling::MethodCode;
		using Method     = telling::Method;
		using Methods    = telling::Methods;
		using Msg        = telling::Msg;
		using MsgView    = telling::MsgView;


	public:
		Reactor(UriView uri_prefix);
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
		virtual Methods allowed(UriView uri) const noexcept = 0;

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

		Uri        _uri_prefix;
		std::mutex _reactor_mutex;
	};
}
