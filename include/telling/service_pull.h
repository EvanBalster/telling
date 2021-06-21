#pragma once


#include <deque>
#include <mutex>
#include <life_lock.h>
#include "io_queue.h"
#include "socket.h"
#include "async_loop.h"
#include "async_queue.h"


namespace telling
{
	// Type hierarchy
	using Pull_Pattern = Communicator::Pattern_Base<Role::SERVICE, Pattern::PUSH_PULL>;
	using Pull_Base    = Pull_Pattern;
	class Pull;     // inherits Pull_Base
	class Pull_Box; // inherits Pull

	// Tag delivered to callbacks
	using Pulling = TagRecv<Pull>;

	// Base class for Pull handlers.
	using AsyncPull = AsyncRecv<Pulling>;



	/*
		Pull communicator that calls an AsyncRecv delegate.
	*/
	class Pull :
		public    Pull_Base,
		protected AsyncRecvLoop<Pulling>
	{
	public:
		/*
			Construct with asynchronous I/O handler and optional socket-sharing.
				Begins listening for messages immediately.
		*/
		Pull()                                                     : Pull_Base(),       AsyncRecvLoop(socketView(),{this}) {}
		Pull(std::weak_ptr<AsyncPull> p)                           : Pull() {initialize(p);}
		Pull(const Pull_Pattern &shared)                           : Pull_Base(shared), AsyncRecvLoop(socketView(),{this}) {}
		Pull(const Pull_Pattern &s, std::weak_ptr<AsyncPull> p)    : Pull(s) {initialize(p);}
		~Pull() {}

		/*
			Start receiving through the provided delegate.
		*/
		void initialize(std::weak_ptr<AsyncPull> p)    {AsyncRecvLoop::recv_start(p);}
	};


	/*
		A Pull communicator with a simple "inbox" queue.
			This is appropriate whenever congestion is not an issue.
	*/
	class Pull_Box : public Pull
	{
	public:
		explicit Pull_Box()                       : Pull()            {_init();}
		Pull_Box(const Pull_Base &shareSocket)    : Pull(shareSocket) {_init();}
		~Pull_Box() {}
			

		/*
			Check for pulled messages.  Non-blocking.
		*/
		bool pull(nng::msg &msg)    {return _queue->pull(msg);}


	protected:
		void _init()    {initialize(_queue.get_weak());}
		edb::life_locked<AsyncRecvQueue<Pulling>> _queue;
	};
}