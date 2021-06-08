#pragma once


#include <deque>
#include <mutex>
#include "io_queue.h"
#include "socket.h"
#include "async.h"
#include "async_queue.h"


namespace telling
{
	namespace service
	{
		// Base type for Push clients.
		using Pull_Base = Communicator::Pattern_Base<Role::SERVICE, Pattern::PUSH_PULL>;


		/*
			Pull communicator that calls an AsyncRecv delegate.
		*/
		class Pull_Async :
			public    Pull_Base,
			protected AsyncRecv::Operator<nng::socket_view>
		{
		public:
			/*
				Construct with an AsyncRecv delegate.
					Begins listening for messages immediately.
			*/
			Pull_Async(std::weak_ptr<AsyncRecv> p = {})                                   : Pull_Base(),            Operator(socketView()) {initialize(p);}
			Pull_Async(const Pull_Base &shareSocket, std::weak_ptr<AsyncRecv> p = {})     : Pull_Base(shareSocket), Operator(socketView()) {initialize(p);}
			~Pull_Async() {}

			/*
				Start receiving through the provided delegate.
			*/
			void initialize(std::weak_ptr<AsyncRecv> p)    {Operator::recv_start(p);}
		};


		/*
			A Pull communicator with a simple "inbox" queue.
				This is appropriate whenever congestion is not an issue.
		*/
		class Pull_Box : public Pull_Async
		{
		public:
			explicit Pull_Box()                       : Pull_Async()            {_init();}
			Pull_Box(const Pull_Base &shareSocket)    : Pull_Async(shareSocket) {_init();}
			~Pull_Box() {}
			

			/*
				Check for pulled messages.  Non-blocking.
			*/
			bool pull(nng::msg &msg)    {return _queue->pull(msg);}


		protected:
			void _init()    {initialize(_queue = std::make_shared<AsyncRecvQueue>());}
			std::shared_ptr<AsyncRecvQueue> _queue;
		};
	}
	
}