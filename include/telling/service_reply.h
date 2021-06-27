#pragma once


#include <utility>
#include <unordered_set>
#include <mutex>
#include <life_lock.h>
#include "io_queue.h"
#include "socket.h"
#include "async_loop.h"


namespace telling
{
	// Type hierarchy
	using Reply_Pattern = Communicator::Pattern_Base<Role::SERVICE, Pattern::REQ_REP>;
	using Reply_Base    = Reply_Pattern;
	class Reply;     // inherits Reply_Base
	class Reply_Box; // inherits Reply

	// Tag delivered to callbacks
	using Replying = TagRespond<Reply>;

	// Base class for asynchronous I/O
	using AsyncRep   = AsyncRespond<Replying>;
	using AsyncReply = AsyncRep;



	/*
		Reply communicator that calls an AsyncReply handler.
	*/
	class Reply : public Reply_Base
	{
	public:
		/*
			Construct with asynchronous I/O handler and optional socket-sharing.
		*/
		Reply()                                                     : Reply_Base() {}
		Reply(std::weak_ptr<AsyncRep> p)                            : Reply() {initialize(p);}
		Reply(const Reply_Pattern &shared)                          : Reply_Base(shared) {}
		Reply(const Reply_Pattern &s, std::weak_ptr<AsyncRep> p)    : Reply(s) {initialize(p);}
		~Reply();

		/*
			Provide a handler for handling requests after construction.
				Throws nng::exception if a handler has already been installed.
		*/
		void initialize(std::weak_ptr<AsyncRep>);


		/*
			Send a response to a specific outstanding query.
		*/
		void respondTo(QueryID, nng::msg &&msg);


	protected:
		std::weak_ptr<AsyncRep> _handler;

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
	class Reply_Box : public Reply
	{
	public:
		explicit Reply_Box();
		Reply_Box(const Reply_Base &shareSocket);
		~Reply_Box();

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
		template<class Fn, class... Args>
		void respond_all(Fn fn, Args&&... args)
		{
			nng::msg req;
			while (receive(req)) respond(fn(req, std::forward<Args>(args) ...));
		}


	protected:
		class Delegate;
		void _init();
		std::shared_ptr<Delegate> _replyBox;

		QueryID current_query = 0;
	};
}
