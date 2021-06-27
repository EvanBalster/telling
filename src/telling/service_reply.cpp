#include <telling/service_reply.h>
#include <nngpp/protocol/rep0.h>


using namespace telling;


/*
	Reply implementation
*/

void Reply::initialize(std::weak_ptr<AsyncReply> new_handler)
{
	if (_handler.lock())
		throw nng::exception(nng::error::busy, "Reply::initialize (already initialized)");

	auto handler = new_handler.lock();
	if (!handler)
		throw nng::exception(nng::error::closed, "Reply::initialize (handler is expired)");

	if (handler)
	{
		_handler = new_handler;

		ctx_aio_recv = make_ctx();

		aio_send = nng::make_aio(&_aioSent, this);
		aio_recv = nng::make_aio(&_aioReceived, this);
		ctx_aio_recv.recv(aio_recv);
	}
}
Reply::~Reply()
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

void Reply::_aioReceived(void *_comm)
{
	auto comm = static_cast<Reply*>(_comm);
	auto &ctx = comm->ctx_aio_recv;
	auto queryID = ctx.get().id;
	auto handler = comm->_handler.lock();

	bool cancel = false;

	// Call handler
	auto error = comm->aio_recv.result();
	if (!handler)
	{
		// No handler; terminate
		comm->aio_recv.release_msg();
		cancel = true;
	}
	else switch (error)
	{
	case nng::error::success:
		{
			{
				// Pickle the context into a QueryID and store it for later.
				std::lock_guard g(comm->unresponded_mtx);
				comm->unresponded.emplace(queryID);
				ctx.release();
			}

			// Deliver asynchronous event...
			nng::msg responseMsg;
			handler->async_recv(
				Replying{comm, queryID, {&responseMsg}},
				std::move(comm->aio_recv.release_msg()));

			// Responding through the tag
			if (responseMsg)
			{
				comm->respondTo(queryID, std::move(responseMsg));
			}
		}
		break;
	case nng::error::canceled:
	case nng::error::timedout:
	default:
		handler->async_error(
			Replying{comm, ctx.get().id}, error);
		cancel = true;
		break;
	}

	if (!cancel)
	{
		// Create a new receive-context and receive another message.
		ctx = comm->make_ctx();
		ctx.recv(comm->aio_recv);
	}
}

void Reply::respondTo(QueryID queryID, nng::msg &&msg)
{
	if (!isReady())
		throw nng::exception(nng::error::closed, "Reply Communicator is not ready.");

	// Allow the handler to prep or reject the message
	if (auto handler = _handler.lock())
	{
		handler->async_prep(Replying{this, queryID}, msg);
	}
	else
	{
		return;
	}

	if (!msg) return;

	// Claim and erase the queryID
	{
		std::lock_guard g(unresponded_mtx);
		if (unresponded.erase(queryID) == 0)
			throw nng::exception(nng::error::inval,
				"respondTo: no outstanding request with this queryID");
	}

	// Unpicle the NNG context and turn it over to sender.
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

void Reply::_aioSent(void *_comm)
{
	auto comm = static_cast<Reply*>(_comm);
	auto handler = comm->_handler.lock();

	// Halt?
	switch (comm->aio_send.result())
	{
	case nng::error::success:  break;
	case nng::error::canceled:
	case nng::error::timedout:
	default:                   return;
	}

	QueryID queryID = comm->ctx_aio_send.get().id;
	handler->async_sent(Replying{comm, queryID});

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


class Reply_Box::Delegate : public AsyncReply
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

	void async_recv(Replying rep, nng::msg &&query) final
	{
		inbox.push(Pending{rep.id, std::move(query)});
	}
	void async_sent(Replying rep) final {}
	void async_error(Replying rep, AsyncError status) final {}
};


Reply_Box::Reply_Box() :
	Reply()
{
	_init();
}
Reply_Box::Reply_Box(const Reply_Pattern &shareSocket) :
	Reply(shareSocket)
{
	_init();
}
Reply_Box::~Reply_Box()
{
}

void Reply_Box::_init()
{
	initialize(_replyBox = std::make_shared<Delegate>());
}


bool Reply_Box::receive(nng::msg  &request)
{
	if (!isReady())
		throw nng::exception(nng::error::closed, "Reply Communicator is not ready.");

	if (current_query != 0)
		throw nng::exception(nng::error::state,
			"Reply: must reply before receiving a new message.");

	Delegate::Pending front;
	if (_replyBox->inbox.pull(front))
	{
		current_query = front.id;
		request       = std::move(front.msg);
		return true;
	}
	else return false;
}

void Reply_Box::respond(nng::msg &&msg)
{
	if (!isReady())
		throw nng::exception(nng::error::closed, "Reply Communicator is not ready.");

	if (current_query == 0)
		throw nng::exception(nng::error::state,
			"Reply: must receive a request before replying.");

	//try
	{
		respondTo(current_query, std::move(msg));
		current_query = 0;
	}
	/*catch (nng::exception e)
	{
		throw e;
	}*/
}
