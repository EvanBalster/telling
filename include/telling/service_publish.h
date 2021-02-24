#pragma once


#include <deque>
#include <mutex>
#include "socket.h"
#include "async.h"
#include "async_queue.h"


namespace telling
{
	namespace service
	{
		/*
			Callback-based 
		*/
		class Publish :
			public    Communicator,
			protected AsyncSend::Operator<nng::socket_view>
		{
		public:
			/*
				Construct with an AsyncSend delegate.
			*/
			Publish(std::shared_ptr<AsyncSend> p)                                 : Communicator(SERVICE, PUB_SUB), Operator(socketView(), p) {}
			Publish(std::shared_ptr<AsyncSend> p, const Publish &shareSocket)     : Communicator(shareSocket),      Operator(socketView(), p) {}
			~Publish() {}

			/*
				Attempt to push a message.  Returns whether accepted by delegate.
			*/
			bool publish(nng::msg &&msg)
			{
				if (!isReady())
					throw nng::exception(nng::error::closed, "Publish Communicator is not ready.");

				return send_msg(std::move(msg));
			}
		};

		/*
			A Publish communicator with a built-in "outbox" queue.
		*/
		class Publish_Outbox : public Publish
		{
		public:
			explicit Publish_Outbox()                     : Publish(std::make_shared<AsyncSendQueue>())              {}
			Publish_Outbox(const Publish &shareSocket)    : Publish(std::make_shared<AsyncSendQueue>(), shareSocket) {}
			~Publish_Outbox() {}
		};
	}
}