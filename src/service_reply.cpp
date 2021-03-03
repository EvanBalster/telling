#include <telling/service_reply.h>
#include <nngpp/protocol/rep0.h>


using namespace telling;
using namespace telling::service;


/*
	Reply implementation
*/

void Rep_Async::_init()
{
	ctx_aio_recv = make_ctx();

	aio_send = nng::make_aio(&_aioSent, this);
	aio_recv = nng::make_aio(&_aioReceived, this);
	ctx_aio_recv.recv(aio_recv);
}
Rep_Async::~Rep_Async()
{
	for (auto id : unresponded)
	{
		// Evil unpickling
		nng_ctx ctx = {id};
		nng_ctx_close(ctx);
	}
	aio_send.stop();
	aio_recv.stop();
	aio_send = nng::aio();
	aio_recv = nng::aio();
}

void Rep_Async::_aioReceived(void *_comm)
{
	auto comm = static_cast<Rep_Async*>(_comm);
	auto &ctx = comm->ctx_aio_recv;
	auto queryID = ctx.get().id;

	AsyncOp::DIRECTIVE directive = AsyncOp::TERMINATE;

	bool await_delayed_response = false;

	// Call delegate
	auto error = comm->aio_recv.result();
	switch (error)
	{
	case nng::error::success:
		{
			AsyncOp::SendDirective react = comm->_delegate->asyncRespond_recv(
				queryID, std::move(comm->aio_recv.release_msg()));
			directive = react.directive;

			if (react.sendMsg)
			{
				// Immediate response
				comm->respondTo(queryID, std::move(react.sendMsg));

				// Dispose of context
				ctx = nng::ctx();
			}
			else
			{
				// Delayed response, decline or terminate...
				await_delayed_response = true;
			}
		}
		break;
	case nng::error::canceled:
	case nng::error::timedout:
	default:
		directive = comm->_delegate->asyncRespond_error(
			ctx.get().id, error);
		break;
	}

	// Handle directive...
	switch (directive)
	{
	default:
	case AsyncOp::AUTO:
		if (error != nng::error::success)
		{
		case AsyncOp::TERMINATE:
			// Stop receiving messages
			return;
		}

		[[fallthrough]];
	case AsyncOp::CONTINUE:
		if (await_delayed_response)
		{
			// Evil pickling
			std::lock_guard g(comm->unresponded_mtx);
			comm->unresponded.emplace(queryID);
			reinterpret_cast<nng_ctx&>(ctx).id = 0;
		}

		[[fallthrough]];
	case AsyncOp::DECLINE:
		// Receive another message with a fresh context
		ctx = nng::make_ctx(comm->socketView());
		ctx.recv(comm->aio_recv);
	}
}

void Rep_Async::respondTo(QueryID queryID, nng::msg &&msg)
{
	if (!isReady())
		throw nng::exception(nng::error::closed, "Reply Communicator is not ready.");

	// Make sure this queryID is awaiting a response
	{
		std::lock_guard g(unresponded_mtx);
		if (unresponded.erase(queryID) == 0)
			throw nng::exception(nng::error::inval,
				"respondTo: no outstanding request with this queryID");
	}

	// Evil unpickling
	nng_ctx _ctx = {queryID};

	// Send or enqueue the reply.
	OutboxItem to_send = {nng::ctx(_ctx), std::move(msg)};
	if (outbox.produce(std::move(to_send)))
	{
		// AIO processing continues
	}
	else
	{
		// Immediately claim the message
		aio_send.set_msg(std::move(to_send.msg));
		ctx_aio_send = std::move(to_send.ctx);
		ctx_aio_send.send(aio_send);
	}
}

void Rep_Async::_aioSent(void *_comm)
{
	auto comm = static_cast<Rep_Async*>(_comm);

	// Halt?
	switch (comm->aio_send.result())
	{
	case nng::error::success:  break;
	case nng::error::canceled:
	case nng::error::timedout:
	default:                   return;
	}

	{
		OutboxItem next;
		if (comm->outbox.consume(next))
		{
			// Transmit another message
			comm->aio_send.set_msg(std::move(next.msg));
			comm->ctx_aio_send   = std::move(next.ctx);
			comm->ctx_aio_send.send(comm->aio_send);
		}
		else
		{
			// AIO sequence completes
		}
	}
}


/*
	Box implementation
*/


class Rep_Box::Delegate : public AsyncRespond
{
public:
	struct Pending
	{
		QueryID  id;
		nng::msg msg;
	};

	RecvQueueMtx_<Pending> inbox;

	Delegate() {}
	~Delegate() {}

	SendDirective asyncRespond_recv(QueryID qid, nng::msg &&query) final
	{
		inbox.push(Pending{qid, std::move(query)});
		return CONTINUE;
	}
	void asyncRespond_done(QueryID qid) final
	{
	}
	Directive asyncRespond_error(QueryID qid, nng::error status) final
	{
		return TERMINATE;
	}
};


Rep_Box::Rep_Box() :
	Rep_Async(std::make_shared<Delegate>())
{

}
Rep_Box::Rep_Box(const Rep_Base &shareSocket) :
	Rep_Async(std::make_shared<Delegate>(), shareSocket)
{
}
Rep_Box::~Rep_Box()
{
}


bool Rep_Box::receive(nng::msg  &request)
{
	if (!isReady())
		throw nng::exception(nng::error::closed, "Reply Communicator is not ready.");

	if (current_query != 0)
		throw nng::exception(nng::error::state,
			"Reply: must reply before receiving a new message.");

	auto delegate = reinterpret_cast<Delegate*>(&*_delegate);

	Delegate::Pending front;
	if (delegate->inbox.pull(front))
	{
		current_query = front.id;
		request       = std::move(front.msg);
		return true;
	}
	else return false;
}

void Rep_Box::respond(nng::msg &&msg)
{
	if (!isReady())
		throw nng::exception(nng::error::closed, "Reply Communicator is not ready.");

	if (current_query == 0)
		throw nng::exception(nng::error::state,
			"Reply: must receive a request before replying.");

	try
	{
		respondTo(current_query, std::move(msg));
		current_query = 0;
	}
	catch (nng::exception e)
	{
		throw e;
	}
}
