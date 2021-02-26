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
		/*
			Base class for PULL communicators with socket-sharing.
				Needs additional code to process I/O.
		*/
		class Pull_Base : public Communicator
		{
		public:
			explicit Pull_Base()                        : Communicator(SERVICE, PUSH_PULL) {}
			Pull_Base(const Pull_Base &shareSocket)     : Communicator(shareSocket)        {}
			~Pull_Base() {}
		};


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
			Pull_Async(std::shared_ptr<AsyncRecv> p)                                   : Pull_Base(),            Operator(socketView(), p) {}
			Pull_Async(std::shared_ptr<AsyncRecv> p, const Pull_Base &shareSocket)     : Pull_Base(shareSocket), Operator(socketView(), p) {}
			~Pull_Async() {}
		};


		/*
			A Pull communicator with a simple "inbox" queue.
				This is appropriate whenever congestion is not an issue.
		*/
		class Pull_Box : public Pull_Async
		{
		public:
			explicit Pull_Box()                       : Pull_Async(std::make_shared<AsyncRecvQueue>())              {}
			Pull_Box(const Pull_Base &shareSocket)    : Pull_Async(std::make_shared<AsyncRecvQueue>(), shareSocket) {}
			~Pull_Box() {}
			

			/*
				Check for pulled messages.  Non-blocking.
			*/
			bool pull(nng::msg &msg)    {return static_cast<AsyncRecvQueue*>(&*recv_delegate())->pull(msg);}
		};
	}
	
}