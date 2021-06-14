#include <unordered_map>

#include <telling/client_request.h>
#include <nngpp/protocol/req0.h>


using namespace telling;
using namespace telling::client;


/*
	Request implementation
*/


struct Req_Async::Action
{
	friend class Request;

	Req_Async* const       request;
	nng::aio               aio;
	nng::ctx               ctx;
	ACTION_STATE           state;
	//std::promise<nng::msg> promise;

	QueryID queryID() const noexcept    {return ctx.get().id;}

	static void _callback(void*);
};


Req_Base::MsgStats Req_Async::msgStats() const noexcept
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


Req_Async::~Req_Async()
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

void Req_Async::initialize(std::weak_ptr<AsyncQuery> delegate)
{
	if (_delegate.lock())
		throw nng::exception(nng::error::busy, "Request_Async::initialize (already initialized)");

	if (delegate.lock())
		_delegate = delegate;
}

QueryID Req_Async::request(nng::msg &&msg)
{
	auto delegate = _delegate.lock();
	if (!isReady())
		throw nng::exception(nng::error::closed, "Request Communicator is not ready.");
	if (!delegate)
		throw nng::exception(nng::error::exist, "Request communicator has no delegate to handle messages");

	Action *action = nullptr;

	// Allocate.
	{
		std::lock_guard<std::mutex> lock(mtx);

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

		auto directive = delegate->asyncQuery_made(action->queryID(), msg);

		switch (directive)
		{
		case Directive::DECLINE:
		case Directive::TERMINATE:
			idle.push_front(action);
			action = nullptr;
			throw nng::exception(nng::error::canceled,
				"AsyncQuery declined the message.");
			break;

		case Directive::AUTO:
		case Directive::CONTINUE:
		default:
			action->state = SEND;
			active.insert(action);
			break;
		}
	} // Release lock

	// Prepare send
	action->aio.set_msg(std::move(msg));
	action->ctx.send(action->aio);

	return action->queryID();
}

void Req_Async::Action::_callback(void *_action)
{
	auto action = static_cast<Req_Async::Action*>(_action);
	auto comm = action->request;
	auto delegate = comm->_delegate.lock();

	Directive directive;
	bool cleanup = false;

	// Errors / callbacks
	auto error = action->aio.result();
	if (!delegate)
	{
		// Terminate communications if delegate is gone
		if (action->state == RECV && error == nng::error::success)
			action->aio.release_msg();
		directive = Directive::TERMINATE;
	}
	else switch (error)
	{
	case nng::error::success:
		switch (action->state)
		{
		case SEND:
			// Send
			directive = delegate->asyncQuery_sent(action->queryID());
			break;
		case RECV:
			// Receive the response
			directive = delegate->asyncQuery_done(action->queryID(), action->aio.release_msg());
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
		directive = delegate->asyncQuery_error(action->queryID(), error);
		cleanup = true;
		break;
	}


	// Cleanup conditions...
	if (directive == Directive::TERMINATE || directive == Directive::DECLINE)
		cleanup = true;

	if (cleanup)
	{
		// Clear AIO message
		action->aio.set_msg(nullptr);
	}


	std::lock_guard<std::mutex> lock(comm->mtx);

	if (cleanup) action->state = IDLE;

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


class Req_Box::Delegate : public AsyncQuery
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

	
	Directive asyncQuery_made (QueryID qid, const nng::msg &query) final
	{
		// TODO could more cheaply allocate nng::msg?
		std::lock_guard g(mtx);
		pending.emplace(qid, Pending{});
		newQueryID = qid;

		return CONTINUE;
	}
	Directive asyncQuery_sent (QueryID qid)                        final
	{
		auto pos = pending.find(qid);
		if (pos != pending.end()) pos->second.sent = true;
		return CONTINUE;
	}
	Directive asyncQuery_done (QueryID qid, nng::msg &&response)   final
	{
		std::lock_guard g(mtx);

		auto pos = pending.find(qid);
		if (pos != pending.end())
		{
			pos->second.promise.set_value(std::move(response));
			pending.erase(pos);
			return CONTINUE;
		}
		else
		{
			return DECLINE;
		}
	}
	Directive asyncQuery_error(QueryID qid, nng::error status)     final
	{
		std::lock_guard g(mtx);
		auto pos = pending.find(qid);
		if (pos != pending.end())
		{
			pos->second.promise.set_exception(std::make_exception_ptr(
				nng::exception(status, pos->second.sent
					? "Request could not be fulfilled."
					: "Request could not be sent.")));
			pending.erase(pos);
		}

		return TERMINATE;
	}
};


Req_Box::Req_Box()                     : Req_Async() {_init();}
Req_Box::Req_Box(const Req_Base &o)    : Req_Async(o) {_init();}
Req_Box::~Req_Box()                    {}

void Req_Box::_init()
{
	_requestBox = std::make_shared<Delegate>();
	initialize(_requestBox);
}

std::future<nng::msg> Req_Box::request(nng::msg &&msg)
{
	auto qid = this->Request_Async::request(std::move(msg));
	return _requestBox->getFuture(qid);
}