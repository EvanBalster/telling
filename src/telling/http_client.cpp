#include <nngpp/aio.h>

#include <telling/http_client.h>


using namespace telling;


struct HttpClient::Action
{
	friend class Request;

	HttpClient* const client;
	QueryID                 queryID;
	nng::aio                aio;
	nng_iov                 iov;
	nng::msg                req;
	nng::msg                res;
	nng::http::conn         conn;
	ACTION_STATE            state;

	HttpRequesting requesting() const noexcept    {return HttpRequesting{client, queryID};}

	static void _callback(void*);
};


HttpClient::MsgStats HttpClient::msgStats() const noexcept
{
	std::lock_guard<std::mutex> g(mtx);

	MsgStats stats = {};

	for (auto &i : active) switch (i->state)
	{
	case SEND: ++stats.awaiting_send; break;
	case RECV: ++stats.awaiting_recv; break;
	default: break;
	}

	return stats;
}


HttpClient::HttpClient(nng::url &&_host, std::weak_ptr<Handler> handler) :
	HttpClient_Base(std::move(_host)),
	client(host)
{
	if (this->host->u_scheme == std::string_view("https"))
	{
		tls = nng::tls::config(nng::tls::mode::client);
		tls.config_auth_mode(nng::tls::auth_mode::none);
		//tls.config_ca_chain()
		//tls. ;
		client.set_tls(tls);
	}

	initialize(handler);
}
HttpClient::~HttpClient()
{
	// Cancel all active AIO
	{
		std::lock_guard<std::mutex> lock(mtx);
		for (auto *action : active) action->aio.cancel();
	}

	// Wait for all AIO to complete.
	while (true)
	{
		Action *action = nullptr;
		{
			std::lock_guard<std::mutex> lock(mtx);
			if (active.size()) action = *active.begin();
		}
		if (action) action->aio.wait();
		else        break;
	}

	// Clean up idle AIO
	for (Action *action : idle)
	{
		delete action;
	}
}


void HttpClient::initialize(std::weak_ptr<Handler> handler)
{
	if (_handler.lock())
		throw nng::exception(nng::error::busy, "HttpClient::initialize (already initialized)");

	if (handler.lock())
		_handler = handler;
}



QueryID HttpClient::request(nng::msg &&req)
{
	auto handler = this->_handler.lock();
	if (!handler)
		throw nng::exception(nng::error::exist, "Request communicator has no message handler");

	std::lock_guard<std::mutex> lock(mtx);

	// Allocate.
	Action *action = nullptr;
	if (idle.empty())
	{
		action = new Action{this};
		action->aio = nng::make_aio(&Action::_callback, action);
	}
	else
	{
		action = idle.front();
		idle.pop_front();
	}

	action->queryID = nextQueryID++;

	handler->async_prep(action->requesting(), req);

	if (!req)
	{
		idle.push_front(action);
		action = nullptr;
		throw nng::exception(nng::error::canceled,
			"AsyncQuery declined the message.");
	}

	action->state = CONNECT;
	active.insert(action);
	
	// Stow request and connect...
	action->req = std::move(req);
	action->client->client.connect(action->aio);

	return action->queryID;
}

void HttpClient::Action::_callback(void *_action)
{
	auto action = static_cast<HttpClient::Action*>(_action);
	auto client = action->client;
	auto handler = client->_handler.lock();

	bool cleanup = false;
	bool cancel  = false;

	// Callbacks / Errors?
	auto error = action->aio.result();
	if (!handler)
	{
		cancel = true;
	}
	else switch (error)
	{
	case nng::error::success:
		switch (action->state)
		{
		case CONNECT:
			action->conn = nng::http::conn(action->aio.get_output<nng_http_conn>(0));
			handler->httpConn_open(action->conn);
			break;
		case SEND:
			// Send
			handler->async_sent(action->requesting());
			action->req = nng::msg();
			break;
		case RECV:
			// Receive the response
			action->res.body().chop(action->res.body().size() - action->aio.count());
			handler->async_recv(action->requesting(), std::move(action->res));
			action->res = nng::msg();
			cleanup = true;
		default:
		case IDLE:
			// Shouldn't happen???
			cleanup = true;
			break;
		}
		break;
	case nng::error::canceled:
	case nng::error::timedout:
	default:
		// Causes the query to be canceled.
		handler->async_error(action->requesting(), error);
		cancel  = true;
		cleanup = true;
		break;
	}


	if (cancel || cleanup)
	{
		if (action->conn)
		{
			// Disconnect
			handler->httpConn_close(action->conn);
			action->conn = nng::http::conn();
		}

		// Clean up
		action->req = nng::msg();
		action->res = nng::msg();
	}


	std::lock_guard<std::mutex> lock(client->mtx);

	if (cancel || cleanup) action->state = IDLE;

	switch (action->state)
	{
	case CONNECT:
		// Connection made; send request.
		action->state = SEND;
		action->iov = nng_iov{action->req.body().get().data(), action->req.body().size()};
		action->aio.set_iov(action->iov);
		//action->aio.set_msg(std::move(action->req));
		action->conn.write(action->aio);
		break;

	case SEND:
		// Request sent; listen for response.
		action->state = RECV;
		action->res = nng::make_msg(4096);
		action->iov = action->iov = nng_iov{action->res.body().get().data(), action->res.body().size()};
		action->aio.set_iov(action->iov);
		action->conn.read(action->aio);
		break;

	case RECV:
		// Message has been received; fulfill promise and return to action pool.
		[[fallthrough]];

	default:
		action->state = IDLE;
		[[fallthrough]];

	case IDLE:
		// Move action to idle queue
		client->active.erase(action);
		client->idle.push_back(action);
	}
}



/*
	Box implementation
*/


class HttpClient_Box::Delegate : public HttpClient::Handler
{
public:
	struct Pending
	{
		bool                   sent = false;
		std::promise<nng::msg> promise;
	};

	std::mutex                           mtx;
	std::unordered_map<QueryID, Pending> pending;

	QueryID newQueryID = 0;

	bool connected = false;

	Delegate() {}
	~Delegate() {}


	std::future<nng::msg> getFuture(QueryID qid)
	{
		std::lock_guard g(mtx);

		auto pos = pending.find(qid);

		if (qid != newQueryID || pos == pending.end())
			throw nng::exception(nng::error::internal, "Inconsistent Query ID");
		newQueryID = 0;

		return pos->second.promise.get_future();
	}


	void httpConn_open  (conn_view conn) final    {connected = true;}
	void httpConn_close (conn_view conn) final    {}

	void async_prep(HttpRequesting req, nng::msg &query) final
	{
		// TODO could more cheaply allocate nng::msg?
		std::lock_guard g(mtx);
		pending.emplace(req.id, Pending{});
		newQueryID = req.id;
	}
	void async_sent(HttpRequesting req) final
	{
		auto pos = pending.find(req.id);
		if (pos != pending.end()) pos->second.sent = true;
	}
	void async_recv(HttpRequesting req, nng::msg &&response) final
	{
		std::lock_guard g(mtx);

		auto pos = pending.find(req.id);
		if (pos != pending.end())
		{
			pos->second.promise.set_value(std::move(response));
			pending.erase(pos);
		}
	}
	void async_error(HttpRequesting req, AsyncError status)     final
	{
		std::lock_guard g(mtx);
		auto pos = pending.find(req.id);
		if (pos != pending.end())
		{
			pos->second.promise.set_exception(std::make_exception_ptr(
				nng::exception(status,
					connected
					? (pos->second.sent
						? "HTTP response reception"
						: "HTTP request transmission")
					: "HTTP connection establishment")));
			pending.erase(pos);
		}
	}
};

HttpClient_Box::HttpClient_Box(nng::url &&_host) :
	HttpClient(std::move(_host))
{
	_init();
}

void HttpClient_Box::_init()
{
	initialize(_httpBox = std::make_shared<Delegate>());
}

std::future<nng::msg> HttpClient_Box::request(nng::msg &&msg)
{
	auto qid = this->HttpClient::request(std::move(msg));
	return _httpBox->getFuture(qid);
}
