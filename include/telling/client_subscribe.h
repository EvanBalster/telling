#pragma once


#include <deque>
#include <mutex>
#include "io_queue.h"
#include "socket.h"
#include "async.h"
#include "async_queue.h"


namespace telling
{
	namespace client
	{
		// Base type for Subscribe clients.
		using Sub_Base = Communicator::Pattern_Base<Role::CLIENT, Pattern::PUB_SUB>;


		// Shorthand & longhand
		using Subscribe_Base                   = Sub_Base;
		class Sub_Async; using Subscribe_Async = Sub_Async;
		class Sub_Box;   using Subscribe_Box   = Sub_Box;


		/*
			Subscribe communicator that calls an AsyncRecv delegate.
		*/
		class Sub_Async :
			public    Sub_Base,
			protected AsyncRecv::Operator<nng::ctx>
		{
		public:
			/*
				Construct with an AsyncRecv delegate.
					Begins listening for messages immediately.
			*/
			Sub_Async(std::shared_ptr<AsyncRecv> p)                                  : Sub_Base(p),           Operator(make_ctx(), p) {}
			Sub_Async(std::shared_ptr<AsyncRecv> p, const Sub_Base &shareSocket)     : Sub_Base(shareSocket), Operator(make_ctx(), p) {}
			~Sub_Async() {}

			/*
				Manage subscriptions.
			*/
			void subscribe  (std::string_view topic);
			void unsubscribe(std::string_view topic);
		};


		/*
			Non-blocking client socket for subscriptions.
		*/
		class Sub_Box : public Sub_Async
		{
		public:
			explicit Sub_Box()                      : Sub_Async(std::make_shared<AsyncRecvQueue>())              {}
			Sub_Box(const Sub_Base &shareSocket)    : Sub_Async(std::make_shared<AsyncRecvQueue>(), shareSocket) {}
			~Sub_Box() {}
			

			/*
				Check for messages from subscribed topics.
					Non-blocking.
			*/
			bool consume(nng::msg &msg)    {return static_cast<AsyncRecvQueue*>(&*recv_delegate())->pull(msg);}
		};
	}
}