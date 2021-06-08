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
		// Base type for Publish services.
		using Pub_Base = Communicator::Pattern_Base<Role::SERVICE, Pattern::PUB_SUB>;


		// Shorthand & longhand
		using Publish_Base                   = Pub_Base;
		class Pub_Async; using Publish_Async = Pub_Async;
		class Pub_Box;   using Publish_Box   = Pub_Box;


		/*
			Publish communicator that calls an AsyncSend delegate.
				Push (AKA "Push_Outbox") includes a delegate suitable for most purposes.
		*/
		class Pub_Async :
			public    Pub_Base,
			protected AsyncSend::Operator<nng::socket_view>
		{
		public:
			/*
				Construct with an AsyncSend delegate.
			*/
			Pub_Async(std::weak_ptr<AsyncSend> p = {})                                  : Pub_Base(),            Operator(socketView()) {initialize(p);}
			Pub_Async(const Pub_Base &shareSocket, std::weak_ptr<AsyncSend> p = {})     : Pub_Base(shareSocket), Operator(socketView()) {initialize(p);}
			~Pub_Async() {}

			/*
				Initialize with the provided delegate
			*/
			void initialize(std::weak_ptr<AsyncSend> p)    {Operator::send_init(p);}

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
		class Pub_Box : public Pub_Async
		{
		public:
			explicit Pub_Box()                      : Pub_Async()            {_init();}
			Pub_Box(const Pub_Base &shareSocket)    : Pub_Async(shareSocket) {_init();}
			~Pub_Box() {}


		protected:
			void _init()    {initialize(_queue = std::make_shared<AsyncSendQueue>());}
			std::shared_ptr<AsyncSendQueue> _queue;
		};

		/*
			Publish_Outbox is so useful we just call it Publish.
		*/
		//using Publish = Publish_Box;
	}
}