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
	using Subscribe_Pattern = Communicator::Pattern_Base<Role::CLIENT, Pattern::PUB_SUB>;
	class Subscribe_Base; // inherits Subscribe_Pattern
	class Subscribe;      // inherits Subscribe_Base
	class Subscribe_Box;  // inherits Subscribe_Box

	// Tag delivered to callbacks
	using Subscribing = TagRecv<Subscribe>;

	// Base class for asynchronous I/O
	using AsyncSub       = AsyncRecv<Subscribing>;
	using AsyncSubscribe = AsyncSub;



	// Base type for Subscribe clients.
	class Subscribe_Base : public Subscribe_Pattern
	{
	public:
		Subscribe_Base()                                        : Subscribe_Pattern()            {}
		Subscribe_Base(const Subscribe_Pattern &shareSocket)    : Subscribe_Pattern(shareSocket) {}
		~Subscribe_Base() {}

		/*
			Manage subscriptions.
		*/
		virtual void subscribe  (std::string_view topic) = 0;
		virtual void unsubscribe(std::string_view topic) = 0;
	};


	/*
		Subscribe communicator that calls an AsyncRecv delegate.
	*/
	class Subscribe :
		public    Subscribe_Base,
		protected AsyncRecvLoop<Subscribing, nng::ctx>
	{
	public:
		/*
			Construct with asynchronous I/O handler and optional socket-sharing.
				Begins listening for messages immediately.
		*/
		Subscribe()                                                         : Subscribe_Base(),       AsyncRecvLoop(make_ctx(),{this}) {}
		Subscribe(std::weak_ptr<AsyncSub> p)                                : Subscribe() {initialize(p);}
		Subscribe(const Subscribe_Pattern &shared)                          : Subscribe_Base(shared), AsyncRecvLoop(make_ctx(),{this}) {}
		Subscribe(const Subscribe_Pattern &s, std::weak_ptr<AsyncSub> p)    : Subscribe(s) {initialize(p);}
		~Subscribe() {}

		/*
			Start receiving through the provided delegate.
		*/
		void initialize(std::weak_ptr<AsyncSub> p)    {AsyncRecvLoop::recv_start(p);}

		/*
			Manage subscriptions.
		*/
		void subscribe  (std::string_view topic) final;
		void unsubscribe(std::string_view topic) final;
	};


	/*
		Non-blocking client socket for subscriptions.
	*/
	class Subscribe_Box : public Subscribe
	{
	public:
		explicit Subscribe_Box()                               : Subscribe()            {_init();}
		Subscribe_Box(const Subscribe_Pattern &shareSocket)    : Subscribe(shareSocket) {_init();}
		~Subscribe_Box() {}
			

		/*
			Check for messages from subscribed topics.
				Non-blocking.
		*/
		bool consume(nng::msg &msg)    {return _queue->pull(msg);}


	protected:
		void _init()    {initialize(_queue.get_weak());}
		edb::life_locked<AsyncRecvQueue<Subscribing>> _queue;
	};
}