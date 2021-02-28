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
		// Base type for Push clients.
		using Push_Base = Communicator::Pattern_Base<Role::CLIENT, Pattern::PUSH_PULL>;


		/*
			Push communicator that calls an AsyncSend delegate.
				Push (AKA "Push_Outbox") includes a delegate suitable for most purposes.
		*/
		class Push_Async :
			public    Push_Base,
			protected AsyncSend::Operator<nng::socket_view>
		{
		public:
			/*
				Construct with an AsyncSend delegate.
			*/
			Push_Async(std::shared_ptr<AsyncSend> p)                                   : Push_Base(),            Operator(socketView(), p) {}
			Push_Async(std::shared_ptr<AsyncSend> p, const Push_Base &shareSocket)     : Push_Base(shareSocket), Operator(socketView(), p) {}
			~Push_Async() {}

			/*
				Attempt to push a message.
					Delegate may throw an exception rather than accepting.
			*/
			void push(nng::msg &&msg)
			{
				if (!isReady()) throw nng::exception(nng::error::closed, "Push Communicator is not ready.");
				send_msg(std::move(msg));
			}
		};


		/*
			A Push communicator with a simple "outbox" queue.
				This is appropriate whenever congestion is not an issue.
		*/
		class Push_Box : public Push_Async
		{
		public:
			explicit Push_Box()                       : Push_Async(std::make_shared<AsyncSendQueue>())              {}
			Push_Box(const Push_Base &shareSocket)    : Push_Async(std::make_shared<AsyncSendQueue>(), shareSocket) {}
			~Push_Box() {}
		};


		/*
			Push_Outbox is so useful we just call it Push.
		*/
		//using Push = Push_Box;
	}
}