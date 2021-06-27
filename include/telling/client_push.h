#pragma once


#include <deque>
#include <mutex>
#include <life_lock.h>
#include "socket.h"
#include "async_loop.h"
#include "async_queue.h"


namespace telling
{
	// Type hierarchy
	using Push_Pattern = Communicator::Pattern_Base<Role::CLIENT, Pattern::PUSH_PULL>;
	using Push_Base    = Push_Pattern;
	class Push;     // inherits Push_Base
	class Push_Box; // inherits Push

	// Tag delivered to callbacks
	using Pushing = TagSend<Push>;

	// Base class for asynchronous I/O
	using  AsyncPush = AsyncSend<Pushing>;



	/*
		Push communicator that calls an AsyncSend handler.
			Push (AKA "Push_Outbox") includes a handler suitable for most purposes.
	*/
	class Push :
		public    Push_Base,
		protected AsyncSendLoop<Pushing>
	{
	public:
		/*
			Construct with asynchronous I/O handler and optional socket-sharing.
		*/
		Push()                                                     : Push_Base(),       AsyncSendLoop(socketView(),{this}) {}
		Push(std::weak_ptr<AsyncPush> p)                           : Push() {initialize(p);}
		Push(const Push_Pattern &shared)                           : Push_Base(shared), AsyncSendLoop(socketView(),{this}) {}
		Push(const Push_Pattern &s, std::weak_ptr<AsyncPush> p)    : Push(s) {initialize(p);}
		~Push() {}

		/*
			Initialize with the provided handler
		*/
		void initialize(std::weak_ptr<AsyncPush> p)    {AsyncSendLoop::send_init(p);}

		/*
			Attempt to push a message.
				Handler may throw an exception rather than accepting.
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
	class Push_Box : public Push
	{
	public:
		explicit Push_Box()                       : Push()            {_init();}
		Push_Box(const Push_Base &shareSocket)    : Push(shareSocket) {_init();}
		~Push_Box() {}


	protected:
		void _init()    {initialize(_queue.get_weak());}
		edb::life_locked<AsyncSendQueue<Pushing>> _queue;
	};
}