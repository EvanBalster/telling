#include <unordered_map>

#include <telling/client_request.h>
#include <nngpp/protocol/req0.h>


using namespace telling;
using namespace telling::client;


/*
	Request implementation
*/

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

QueryID Req_Async::request(nng::msg &&msg)
{
	if (!isReady())
		throw nng::exception(nng::error::closed, "Push Communicator is not ready.");

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

		auto directive = _delegate->asyncQuery_made(action->queryID(), msg);

		switch (directive)
		{
		case AsyncOp::DECLINE:
		case AsyncOp::TERMINATE:
			idle.push_front(action);
			action = nullptr;
			throw nng::exception(nng::error::canceled,
				"AsyncQuery declined the message.");
			break;

		case AsyncOp::AUTO:
		case AsyncOp::CONTINUE:
		default:
			action->state = SEND;
			active.insert(action);
			break;
		}
	};

	// Prepare send
	action->aio.set_msg(std::move(msg));
	action->ctx.send(action->aio);

	return action->queryID();
}

void Req_Async::Action::_callback(void *_action)
{
	auto action = static_cast<Req_Async::Action*>(_action);
	auto comm = action->request;
	auto &delegate = comm->_delegate;

	AsyncOp::DIRECTIVE directive;

	// Errors?
	auto error = action->aio.result();
	switch (error)
	{
	case nng::error::success:  break;
	case nng::error::canceled:
	case nng::error::timedout:
	default:
		// Causes the query to be canceled.
		directive = delegate->asyncQuery_error(action->queryID(), error);
		break;
	}

	// Deliver callbacks
	switch (action->state)
	{
	case SEND:
		// Send
		directive = delegate->asyncQuery_sent(action->queryID());
		break;
	case RECV:
		// Receive the response
		directive = delegate->asyncQuery_done(action->queryID(), action->aio.release_msg());
	default:
	case IDLE:
		// Shouldn't happen???
		break;
	}

	std::lock_guard<std::mutex> lock(comm->mtx);

	if (error != nng::error::success)
	{
		action->state = IDLE;
	}
	else switch (directive)
	{
	case AsyncOp::AUTO:
	case AsyncOp::CONTINUE:
		break;
	case AsyncOp::DECLINE:
	case AsyncOp::TERMINATE:
		action->state = IDLE;
		break;
	}

	switch (action->state)
	{
	case SEND:
		// Message has been sent; prepare to receive another.
		action->state = RECV;
		action->ctx.recv(action->aio);
		break;

	case RECV:
		// Message has been received; fulfill promise and return to action pool.
		action->aio.set_msg(nullptr);
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


Req_Box::Req_Box()                     : Req_Async(std::make_shared<Delegate>()) {}
Req_Box::Req_Box(const Req_Base &o)    : Req_Async(std::make_shared<Delegate>(), o) {}
Req_Box::~Req_Box()                    {}

std::future<nng::msg> Req_Box::request(nng::msg &&msg)
{
	auto qid = this->Request_Async::request(std::move(msg));
	return static_cast<Delegate*>(&*_delegate)->getFuture(qid);
}