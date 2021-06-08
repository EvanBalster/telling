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
		using Sub_Pattern = Communicator::Pattern_Base<Role::CLIENT, Pattern::PUB_SUB>;

		// Base type for Subscribe clients.
		class Sub_Base : public Sub_Pattern
		{
		public:
			Sub_Base()                               : Sub_Pattern()            {}
			Sub_Base(const Sub_Base &shareSocket)    : Sub_Pattern(shareSocket) {}
			~Sub_Base() {}

			/*
				Manage subscriptions.
			*/
			virtual void subscribe  (std::string_view topic) = 0;
			virtual void unsubscribe(std::string_view topic) = 0;
		};


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
			Sub_Async(std::weak_ptr<AsyncRecv> p = {})                                  : Sub_Base(),            Operator(make_ctx()) {initialize(p);}
			Sub_Async(const Sub_Base &shareSocket, std::weak_ptr<AsyncRecv> p = {})     : Sub_Base(shareSocket), Operator(make_ctx()) {initialize(p);}
			~Sub_Async() {}

			/*
				Start receiving through the provided delegate.
			*/
			void initialize(std::weak_ptr<AsyncRecv> p)    {Operator::recv_start(p);}

			/*
				Manage subscriptions.
			*/
			void subscribe  (std::string_view topic) final;
			void unsubscribe(std::string_view topic) final;
		};


		/*
			Non-blocking client socket for subscriptions.
		*/
		class Sub_Box : public Sub_Async
		{
		public:
			explicit Sub_Box()                      : Sub_Async()            {_init();}
			Sub_Box(const Sub_Base &shareSocket)    : Sub_Async(shareSocket) {_init();}
			~Sub_Box() {}
			

			/*
				Check for messages from subscribed topics.
					Non-blocking.
			*/
			bool consume(nng::msg &msg)    {return _queue->pull(msg);}


		protected:
			void _init()    {initialize(_queue = std::make_shared<AsyncRecvQueue>());}
			std::shared_ptr<AsyncRecvQueue> _queue;
		};
	}
}