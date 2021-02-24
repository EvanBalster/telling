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
		/*
			Callback-based client for subscriptions.
		*/
		class Subscribe :
			public    Communicator,
			protected AsyncRecv::Operator<nng::ctx>
		{
		public:
			/*
				Construct with an AsyncRecv delegate.
					Begins listening for messages immediately.
			*/
			Subscribe(std::shared_ptr<AsyncRecv> p)                                   : Communicator(CLIENT, PUB_SUB), Operator(make_ctx(), p) {}
			Subscribe(std::shared_ptr<AsyncRecv> p, const Subscribe &shareSocket)     : Communicator(shareSocket),     Operator(make_ctx(), p) {}
			~Subscribe() {}

			/*
				Manage subscriptions.
			*/
			void subscribe  (std::string_view topic);
			void unsubscribe(std::string_view topic);
		};


		/*
			Non-blocking client socket for subscriptions.
		*/
		class Subscribe_Inbox : public Subscribe
		{
		public:
			explicit Subscribe_Inbox()                       : Subscribe(std::make_shared<AsyncRecvQueue>())              {}
			Subscribe_Inbox(const Subscribe &shareSocket)    : Subscribe(std::make_shared<AsyncRecvQueue>(), shareSocket) {}
			~Subscribe_Inbox() {}
			

			/*
				Check for messages from subscribed topics.
					Non-blocking.
			*/
			bool consume(nng::msg &msg)    {return static_cast<AsyncRecvQueue*>(&*recv_delegate())->pull(msg);}
		};
	}
}