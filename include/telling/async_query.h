#pragma once


#include "async.h"
#include <nngpp/aio.h>
#include <nngpp/ctx.h>


/*
	This header defines asynchronous interfaces for queries and responses.
		These may be used in request-reply and surveyor-respondent patterns.
*/


namespace telling
{
	/*
		QueryID may be used for tracking 
			Unique until a "done" or "error" event after which it may be re
	*/
	using QueryID = decltype(nng_ctx::id);


	/*
		Callback interface for sending queries and getting responses.
			Used for REQuest protocol, maybe Surveyor in the future.
	*/
	class AsyncQuery : public AsyncOp
	{
	public:
		class Context
		{
		public:
			virtual ~Context() {}

			// Get a unique ID for this query.
			virtual uint32_t queryID() const noexcept = 0;
		};


	public:
		virtual ~AsyncQuery() {}

		/*
			Asynchronous events.
				"made" notes a newly-composed request, which may be declined.
				"sent" notifies that one message has finished sending.
				"done" delivers the response.
				"error" signals that the query has failed or terminated.

			QueryID may be reused after "done" or "error".
		*/
		virtual Directive asyncQuery_made (QueryID, const nng::msg &query)  {return CONTINUE;}
		virtual Directive asyncQuery_sent (QueryID)                         {return CONTINUE;}
		virtual Directive asyncQuery_done (QueryID, nng::msg &&response)    = 0;
		virtual Directive asyncQuery_error(QueryID, nng::error status)      = 0;
	};


	/*
		Callback interface for responding to messages.
			Used for REPly protocol, maybe Responder in the future.
	*/
	class AsyncRespond : public AsyncOp
	{
	public:
		virtual ~AsyncRespond() {}

		/*
			Asynchronous events.
				"recv" delivers a new query, providing for immediate or later response.
				"done" signals that the response has been sent.
				"error" signals that the query has failed or terminated.

			QueryID may be reused after "done" or "error".

			If a message is not returned from "recv", behavior is caller-dependent;
				Where possible, communicators should offer a method for delayed response.
		*/
		virtual SendDirective asyncRespond_recv (QueryID, nng::msg &&query)  = 0;
		virtual void          asyncRespond_done (QueryID)                    = 0;
		virtual     Directive asyncRespond_error(QueryID, nng::error status) = 0;
	};
}