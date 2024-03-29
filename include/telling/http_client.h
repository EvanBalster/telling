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

#include "async_loop.h"
#include "msg_view.h"
#include "host_address.h"


namespace telling
{
	class HttpClient_Base;
	class HttpClient;      // inherits HttpClient_Base
	class HttpClient_Box;  // inherits HttpClient

	// Tag delivered to callbacks
	struct HttpRequesting
	{
		HttpClient *comm;
		QueryID     id;
	};

	// Base class for asynchronous I/O
	using AsyncHttpReq     = AsyncQuery<HttpRequesting>;
	using AsyncHttpRequest = AsyncHttpReq;


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
		virtual ~HttpClient_Base()           {}

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
	class HttpClient : public HttpClient_Base
	{
	public:
		using conn_view = nng::http::conn_view;

		class Handler : public AsyncHttpRequest
		{
		public:
			virtual ~Handler() {}

			virtual void      httpConn_open  (conn_view conn) {}
			virtual void      httpConn_close (conn_view conn) {}

			// Optional progress notification
			virtual void async_response_progress(HttpRequesting, MsgCompletion, const MsgView::Reply&) {}

			void async_prep (HttpRequesting, nng::msg &query) override     {}
			void async_sent (HttpRequesting)                  override     {}
			//void async_recv (HttpRequesting, nng::msg &&response)   override     = 0;
			//void async_error(HttpRequesting, nng::error status)     override     = 0;
		};


	public:
		// Construct with URL of host.
		HttpClient(nng::url &&_host, std::weak_ptr<Handler> = {});
		~HttpClient() override;


		/*
			Install a handler after construction.
				Throws nng::exception if a handler is already installed.
		*/
		void initialize(std::weak_ptr<Handler>);


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

		std::weak_ptr<Handler> _handler;

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
	class HttpClient_Box : public HttpClient
	{
	public:
		explicit HttpClient_Box(nng::url &&_host);
		~HttpClient_Box() override                    {}

		/*
			Send a request to the server.
		*/
		std::future<nng::msg> request(nng::msg &&req);


	protected:
		class Delegate;
		void _init();
		std::shared_ptr<Delegate> _httpBox;
	};
}
