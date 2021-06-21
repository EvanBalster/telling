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
	using Publish_Pattern = Communicator::Pattern_Base<Role::SERVICE, Pattern::PUB_SUB>;
	using Publish_Base    = Publish_Pattern;
	class Publish;     // inherits Publish_Base
	class Publish_Box; // inherits Publish

	// Tag delivered to callbacks
	using Publishing = TagSend<Publish>;

	// Base class for Push handlers.
	using AsyncPub     = AsyncSend<Publishing>;
	using AsyncPublish = AsyncPub;


	/*
		Publish communicator that calls an AsyncSend delegate.
			Push (AKA "Push_Outbox") includes a delegate suitable for most purposes.
	*/
	class Publish :
		public    Publish_Base,
		protected AsyncSendLoop<Publishing>
	{
	public:
		/*
			Construct with asynchronous I/O handler and optional socket-sharing.
		*/
		Publish()                                                       : Publish_Base(),       AsyncSendLoop(socketView(),{this}) {}
		Publish(std::weak_ptr<AsyncPub> p)                              : Publish() {initialize(p);}
		Publish(const Publish_Pattern &shared)                          : Publish_Base(shared), AsyncSendLoop(socketView(),{this}) {}
		Publish(const Publish_Pattern &s, std::weak_ptr<AsyncPub> p)    : Publish(s) {initialize(p);}
		~Publish() {}

		/*
			Initialize with the provided delegate
		*/
		void initialize(std::weak_ptr<AsyncPub> p)    {AsyncSendLoop::send_init(p);}

		/*
			Attempt to push a message.  Throws nng::exception on failure.
		*/
		void publish(nng::msg &&msg)
		{
			if (!isReady()) throw nng::exception(nng::error::closed, "Publish Communicator is not ready.");
			send_msg(std::move(msg));
		}
	};

	/*
		A Publish communicator with a simple "outbox" queue.
			This is appropriate whenever congestion is not an issue.
	*/
	class Publish_Box : public Publish
	{
	public:
		explicit Publish_Box()                          : Publish()            {_init();}
		Publish_Box(const Publish_Base &shareSocket)    : Publish(shareSocket) {_init();}
		~Publish_Box() {}


	protected:
		void _init()    {initialize(_queue.get_weak());}
		edb::life_locked<AsyncSendQueue<Publishing>> _queue;
	};
}