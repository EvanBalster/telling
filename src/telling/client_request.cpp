#include <unordered_map>

#include <telling/client_request.h>
#include <nngpp/protocol/req0.h>


using namespace telling;


/*
	Request implementation
*/


struct Request::Action
{
	friend class Request;

	Request* const         request;
	nng::aio               aio;
	nng::ctx               ctx;
	ACTION_STATE           state;
	//std::promise<nng::msg> promise;

	QueryID    queryID()    const noexcept    {return ctx.get().id;}
	Requesting requesting() const noexcept    {return Requesting{request, queryID()};}

	static void _callback(void*);
};


Request_Base::MsgStats Request::msgStats() const noexcept
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


Request::~Request()
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

void Request::initialize(std::weak_ptr<AsyncRequest> new_handler)
{
	if (_handler.lock())
		throw nng::exception(nng::error::busy, "Request::initialize (already initialized)");

	if (!new_handler.lock())
		throw nng::exception(nng::error::closed, "Request::initialize (handler is expired)");
	
	_handler = new_handler;
}

QueryID Request::request(nng::msg &&msg)
{
	auto handler = _handler.lock();
	if (!isReady())
		throw nng::exception(nng::error::closed, "Request Communicator is not ready.");
	if (!handler)
		throw nng::exception(nng::error::exist, "Request communicator has no message handler");

	std::lock_guard<std::mutex> lock(mtx);

	// Allocate.
	Action *action = nullptr;
	if (idle.empty())
	{
		action = new Action{this};
		action->ctx = make_ctx();
		action->aio = nng::make_aio(&Action::_callback, action);
	}
	else
	{
		action = idle.front();
		idle.pop_front();
	}

	handler->async_prep(action->requesting(), msg);

	if (!msg)
	{
		idle.push_front(action);
		action = nullptr;
		throw nng::exception(nng::error::canceled,
			"AsyncQuery declined the message.");
	}

	// Proceed
	action->state = SEND;
	active.insert(action);

	// Prepare send
	action->aio.set_msg(std::move(msg));
	action->ctx.send(action->aio);

	return action->queryID();
}

void Request::Action::_callback(void *_action)
{
	auto action = static_cast<Request::Action*>(_action);
	auto comm = action->request;

	bool cleanup = false;
	bool cancel  = false;

	// Errors / callbacks
	auto error = action->aio.result();
	{
		auto handler = comm->_handler.lock();

		if (!handler)
		{
			// Terminate communications if handler is gone
			if (action->state == RECV && error == nng::error::success)
				action->aio.release_msg();
			cleanup = true;
			cancel = true;
		}
		else switch (error)
		{
		case nng::error::success:
			switch (action->state)
			{
			case SEND:
				// Sent!
				handler->async_sent(action->requesting());
				break;
			case RECV:
				// Receive the response
				handler->async_recv(action->requesting(), action->aio.release_msg());
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
			cleanup = true;
			cancel = true;
			break;
		}
	}

	if (cancel || cleanup)
	{
		// Clear AIO message.  Possibly cancels the operation?
		action->aio.set_msg(nullptr);

		//TODO // Does this leave ctx in receiving state??
	}


	std::lock_guard<std::mutex> lock(comm->mtx);

	if (cancel || cleanup) action->state = IDLE;

	switch (action->state)
	{
	case SEND:
		// Request sent; listen for response.
		action->state = RECV;
		action->ctx.recv(action->aio);
		break;

	case RECV:
		// Message has been received; fulfill promise and return to action pool.
		[[fallthrough]];

	default:
		action->state = IDLE;
		[[fallthrough]];

	case IDLE:
		// Move action to idle queue
		comm->active.erase(action);
		comm->idle.push_back(action);
	}	
}



/*
	Box implementation
*/


class Request_Box::Delegate : public AsyncRequest
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

	
	void async_prep(Requesting req, nng::msg &query) final
	{
		// TODO could more cheaply allocate nng::msg?
		std::lock_guard g(mtx);
		pending.emplace(req.id, Pending{});
		newQueryID = req.id;
	}
	void async_sent(Requesting req)                        final
	{
		auto pos = pending.find(req.id);
		if (pos != pending.end()) pos->second.sent = true;
	}
	void async_recv(Requesting req, nng::msg &&response)   final
	{
		std::lock_guard g(mtx);

		auto pos = pending.find(req.id);
		if (pos != pending.end())
		{
			pos->second.promise.set_value(std::move(response));
			pending.erase(pos);
		}
	}
	void async_error(Requesting req, AsyncError status)     final
	{
		std::lock_guard g(mtx);
		auto pos = pending.find(req.id);
		if (pos != pending.end())
		{
			pos->second.promise.set_exception(std::make_exception_ptr(
				nng::exception(status, pos->second.sent
					? "Request could not be fulfilled."
					: "Request could not be sent.")));
			pending.erase(pos);
		}
	}
};


Request_Box::Request_Box()                     : Request() {_init();}
Request_Box::Request_Box(const Request_Base &o)    : Request(o) {_init();}
Request_Box::~Request_Box()                    {}

void Request_Box::_init()
{
	_requestBox = std::make_shared<Delegate>();
	initialize(_requestBox);
}

std::future<nng::msg> Request_Box::request(nng::msg &&msg)
{
	auto qid = this->Request::request(std::move(msg));
	return _requestBox->getFuture(qid);
}