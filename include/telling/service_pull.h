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
			Callback-based client for pull messages.
		*/
		class Pull :
			public    Communicator,
			protected AsyncRecv::Operator<nng::socket_view>
		{
		public:
			/*
				Construct with an AsyncRecv delegate.
					Begins listening for messages immediately.
			*/
			Pull(std::shared_ptr<AsyncRecv> p)                              : Communicator(SERVICE, PUSH_PULL), Operator(socketView(), p) {}
			Pull(std::shared_ptr<AsyncRecv> p, const Pull &shareSocket)     : Communicator(shareSocket),        Operator(socketView(), p) {}
			~Pull() {}
		};


		/*
			Non-blocking client socket for subscriptions.
		*/
		class Pull_Inbox : public Pull
		{
		public:
			explicit Pull_Inbox()                  : Pull(std::make_shared<AsyncRecvQueue>())              {}
			Pull_Inbox(const Pull &shareSocket)    : Pull(std::make_shared<AsyncRecvQueue>(), shareSocket) {}
			~Pull_Inbox() {}
			

			/*
				Check for pulled messages.  Non-blocking.
			*/
			bool pull(nng::msg &msg)    {return static_cast<AsyncRecvQueue*>(&*recv_delegate())->pull(msg);}
		};
	}
	
}