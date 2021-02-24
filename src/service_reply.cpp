#include <telling/service_reply.h>
#include <nngpp/protocol/rep0.h>


using namespace telling;
using namespace telling::service;


/*
	Reply implementation
*/

void Reply::_onOpen()
{
	ctx_aio_recv = make_ctx();

	aio_send = nng::make_aio(&_aioSent, this);
	aio_recv = nng::make_aio(&_aioReceived, this);
	ctx_aio_recv.recv(aio_recv);
}
Reply::~Reply()
{
	aio_send.stop();
	aio_recv.stop();
	aio_send = nng::aio();
	aio_recv = nng::aio();
}

void Reply::_aioReceived(void *_comm)
{
	auto comm = static_cast<Reply*>(_comm);

	// Halt?
	switch (comm->aio_recv.result())
	{
	case nng::error::success:  break;
	case nng::error::canceled:
	case nng::error::timedout:
	default:                   return;
	}

	// Add message to queue.
	comm->inbox.push(Pending{
		std::move(comm->ctx_aio_recv),
		std::move(comm->aio_recv.release_msg())});

	// Receive another message, creating a fresh context.
	comm->ctx_aio_recv = nng::make_ctx(comm->socketView());
	comm->ctx_aio_recv.recv(comm->aio_recv);
}

bool Reply::receive(nng::msg  &request)
{
	if (ctx_api_received)
		throw nng::exception(nng::error::state,
			"Reply: must reply before receiving a new message.");

	Pending front;
	if (inbox.pull(front))
	{
		ctx_api_received = std::move(front.ctx);
		request          = std::move(front.msg);
		return true;
	}
	else
	{
		return false;
	}
}

void Reply::respond(nng::msg &&msg)
{
	if (!isReady())
		throw nng::exception(nng::error::closed, "Push Communicator is not ready.");

	if (!ctx_api_received)
		throw nng::exception(nng::error::state,
			"Reply: must receive a request before replying.");

	Pending to_send = {std::move(ctx_api_received), std::move(msg)};
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

	// Halt?
	switch (comm->aio_send.result())
	{
	case nng::error::success:  break;
	case nng::error::canceled:
	case nng::error::timedout:
	default:                   return;
	}

	{
		Pending next;
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
