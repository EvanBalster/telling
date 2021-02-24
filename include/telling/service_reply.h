#pragma once


#include <deque>
#include <mutex>
#include "io_queue.h"
#include "socket.h"


namespace telling
{
	namespace service
	{
		/*
			Non-blocking service socket for replying to requests.
		*/
		class Reply : public Communicator
		{
		public:
			explicit Reply()         : Communicator(SERVICE, REQ_REP) {_onOpen();}
			Reply(const Reply &o)    : Communicator(o)                {_onOpen();}
			~Reply();

			/*
				Requests should be processed one-by-one.
					Following a successful receive, a reply must be sent before the next receive.
					Replies may only be sent after a receive.
			*/
			bool receive(nng::msg  &request);
			void respond(nng::msg &&reply);


			/*
				Automatically loop through requests and reply to them with a functor.
			*/
			template<class Fn>
			void respond_all(Fn fn)           {nng::msg req; while (receive(req)) respond(fn(req));}
			template<class Fn, class A>
			void respond_all(Fn fn, A arg)    {nng::msg req; while (receive(req)) respond(fn(arg, req));}


		protected:
			struct Pending
			{
				nng::ctx ctx;
				nng::msg msg;
			};

			nng::aio               aio_send, aio_recv;
			RecvQueueMtx_<Pending> inbox;
			SendQueueMtx_<Pending> outbox;
			
			nng::ctx ctx_aio_recv, ctx_aio_send;
			nng::ctx ctx_api_received;

			void _onOpen();
			static void _aioReceived(void*);
			static void _aioSent    (void*);
		};
	}

}