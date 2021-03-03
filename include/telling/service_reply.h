#pragma once


#include <unordered_set>
#include <mutex>
#include "io_queue.h"
#include "socket.h"
#include "async_query.h"


namespace telling
{
	namespace service
	{
		// Base type for Reply services.
		using Rep_Base = Communicator::Pattern_Base<Role::SERVICE, Pattern::REQ_REP>;


		// Shorthand & longhand
		using                  Reply_Base  = Rep_Base;
		class Rep_Async; using Reply_Async = Rep_Async;
		class Rep_Box;   using Reply_Box   = Rep_Box;


		/*
			Reply communicator that calls an AsyncRespond delegate.
		*/
		class Rep_Async :
			public Rep_Base
		{
		public:
			Rep_Async(std::shared_ptr<AsyncRespond> p)                                 : Rep_Base(p),           _delegate(p)    {_init();}
			Rep_Async(std::shared_ptr<AsyncRespond> p, const Rep_Base &shareSocket)    : Rep_Base(shareSocket), _delegate(p)    {_init();}
			~Rep_Async();


			/*
				Send a response to a specific outstanding query.
			*/
			void respondTo(QueryID, nng::msg &&msg);


		protected:
			std::shared_ptr<AsyncRespond> _delegate;

			struct OutboxItem
			{
				nng::ctx ctx;
				nng::msg msg;
			};
			using Unresponded = std::unordered_set<QueryID>;

			std::mutex  unresponded_mtx;
			Unresponded unresponded;

			nng::aio                  aio_send, aio_recv;
			SendQueueMtx_<OutboxItem> outbox;
			
			nng::ctx ctx_aio_recv, ctx_aio_send;

			void _init();
			static void _aioReceived(void*);
			static void _aioSent    (void*);
		};



		/*
			Non-blocking service socket for replying to requests.
		*/
		class Rep_Box : public Rep_Async
		{
		public:
			explicit Rep_Box();
			Rep_Box(const Rep_Base &shareSocket);
			~Rep_Box();

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
			template<class Fn>                 void respond_all(Fn fn)                  {nng::msg req; while (receive(req)) respond(fn(req));}
			template<class Fn, class UserData> void respond_all(Fn fn, UserData arg)    {nng::msg req; while (receive(req)) respond(fn(arg, req));}


		protected:
			class Delegate;

			QueryID current_query = 0;
		};
	}

}