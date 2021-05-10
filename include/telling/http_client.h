#pragma once


#include <mutex>
#include <future>
#include <deque>
#include <unordered_set>

#include <nngpp/http/client.h>
#include <nngpp/http/conn.h>
#include <nngpp/http/res.h>
#include <nngpp/transport/tls.h>
#include <nngpp/transport/tls/config.h>
#include <nngpp/url.h>

#include "async_query.h"
#include "host_address.h"


namespace telling
{
	/*
		Base type for HTTP clients.
	*/
	class HttpClient_Base
	{
	public:
		const nng::url host;


	public:
		// Construct with URL of host.
		HttpClient_Base(nng::url &&_host)    : host(std::move(_host)) {}

		/*
			Get statistics
		*/
		struct MsgStats
		{
			size_t
				awaiting_send,
				awaiting_recv;
		};

		virtual MsgStats msgStats() const noexcept = 0;
	};

	/*
		HTTP Client with asynchronous events.
	*/
	class HttpClient_Async : public HttpClient_Base
	{
	public:
		using conn_view = nng::http::conn_view;

		class Handler : public AsyncOp
		{
		public:
			virtual ~Handler() {}

			virtual void      httpConn_open  (conn_view conn) {}
			virtual void      httpConn_close (conn_view conn) {}

			virtual Directive httpQuery_made (QueryID, const nng::msg &query)     {return CONTINUE;}
			virtual Directive httpQuery_sent (QueryID)                            {return CONTINUE;}
			virtual Directive httpQuery_done (QueryID, nng::msg &&response)       = 0;
			virtual Directive httpQuery_error(QueryID, nng::error status)         = 0;

			// TODO allow chaining queries from httpQuery_done
		};


	public:
		// Construct with URL of host.
		HttpClient_Async(std::shared_ptr<Handler>, nng::url &&_host);
		~HttpClient_Async();


		/*
			Initiate a request.
				May fail, throwing nng::exception.
		*/
		QueryID request(nng::msg &&msg);


		/*
			Stats implementation
		*/
		MsgStats msgStats() const noexcept final;


	protected:
		nng::http::client client;
		nng::tls::config  tls;

		std::shared_ptr<Handler> _handler;

		enum ACTION_STATE
		{
			IDLE    = 0,
			CONNECT = 1,
			SEND    = 2,
			RECV    = 3,
		};

		struct Action;
		friend struct Action;
		QueryID                     nextQueryID = 0;
		mutable std::mutex          mtx;
		std::unordered_set<Action*> active;
		std::deque<Action*>         idle;
	};


	/*
		Non-blocking client socket for requests.
			Supports multiple pending requests.
			Requests are handled with std::future.
	*/
	class HttpClient_Box : public HttpClient_Async
	{
	public:
		explicit HttpClient_Box(nng::url &&_host);
		~HttpClient_Box()                            {}

		/*
			Send a request to the server.
		*/
		std::future<nng::msg> request(nng::msg &&req);


	protected:
		class Delegate;
	};
}
