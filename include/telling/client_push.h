#pragma once


#include <deque>
#include <mutex>
#include "socket.h"
#include "async.h"
#include "async_queue.h"


namespace telling
{
	namespace client
	{
		/*
			Callback-based push communicator.
		*/
		class Push :
			public    Communicator,
			protected AsyncSend::Operator<nng::socket_view>
		{
		public:
			/*
				Construct with an AsyncSend delegate.
			*/
			Push(std::shared_ptr<AsyncSend> p)                              : Communicator(CLIENT, PUSH_PULL), Operator(socketView(), p) {}
			Push(std::shared_ptr<AsyncSend> p, const Push &shareSocket)     : Communicator(shareSocket),       Operator(socketView(), p) {}
			~Push() {}

			/*
				Attempt to push a message.  Returns whether accepted by delegate.
			*/
			bool push(nng::msg &&msg)
			{
				if (!isReady())
					throw nng::exception(nng::error::closed, "Push Communicator is not ready.");

				return send_msg(std::move(msg));
			}
		};

		/*
			A Push communicator with a built-in "outbox" queue.
		*/
		class Push_Outbox : public Push
		{
		public:
			explicit Push_Outbox()                  : Push(std::make_shared<AsyncSendQueue>())              {}
			Push_Outbox(const Push &shareSocket)    : Push(std::make_shared<AsyncSendQueue>(), shareSocket) {}
			~Push_Outbox() {}
		};
	}
}