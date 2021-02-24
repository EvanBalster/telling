#include <iostream>

#include <telling/client_request.h>
#include <nngpp/protocol/req0.h>


using namespace telling;
using namespace telling::client;


/*
	Request implementation
*/

size_t Request::countUnsent()
{
	std::lock_guard<std::mutex> g(mtx);

	size_t n = 0;
	for (auto &i : active) if (i->state == SEND) ++n;
	return n;
}
size_t Request::countSentAwaitingReply()
{
	std::lock_guard<std::mutex> g(mtx);

	size_t n = 0;
	for (auto &i : active) if (i->state == RECV) ++n;
	return n;
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

std::future<nng::msg> Request::request(nng::msg &&msg)
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
		
		action->state = SEND;
		active.insert(action);
	};

	// Prepare send
	action->aio.set_msg(std::move(msg));
	action->ctx.send(action->aio);

	return action->promise.get_future();
}

void Request::Action::_callback(void *_action)
{
	auto action = static_cast<Request::Action*>(_action);

	// Halt?
	bool ok = false;
	switch (auto err = action->aio.result())
	{
	case nng::error::success:  break;
	case nng::error::canceled:
	case nng::error::timedout:
	default:
		// Causes the query to be canceled.
		std::cout << "[REQ client AIO " << nng::to_string(err) << "]\n" << std::flush;
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
		action->promise.set_value(nng::msg(action->aio.get_msg().get()));
		action->aio.set_msg(nullptr);
		std::cout << "[REQ client AIO finished]\n" << std::flush;
		[[fallthrough]];

	default:
		action->state = IDLE;
		[[fallthrough]];

	case IDLE:
		// Release the promise, abandoning it if the operation was canceled or timed out.
		action->promise = std::promise<nng::msg>();

		// Move action to idle queue
		std::lock_guard<std::mutex> lock(action->request->mtx);
		action->request->active.erase(action);
		action->request->idle.push_back(action);
	}	
}