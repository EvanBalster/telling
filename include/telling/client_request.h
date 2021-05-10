#pragma once


#include <deque>
#include <mutex>
#include <future>
#include <robin_hood.h>
#include <unordered_set>
#include "socket.h"

#include "async_query.h"


namespace telling
{
	namespace client
	{
		// Base type for Request clients.
		using Req_Pattern = Communicator::Pattern_Base<Role::CLIENT, Pattern::REQ_REP>;

		// Base type for Request clients.
		class Req_Base : public Req_Pattern
		{
		public:
			Req_Base(std::shared_ptr<AsyncOp_withPipeEvents> p)    : Req_Pattern(p)           {}
			Req_Base(const Req_Base &shareSocket)                  : Req_Pattern(shareSocket) {}
			~Req_Base() {}
			
			
			/*
				Get statistics
			*/
			struct MsgStats
			{
				size_t
					awaiting_send,
					awaiting_recv;
			};

			virtual MsgStats msgStats() const noexcept = 0;
		};


		// Shorthand & longhand
		using                  Request_Base  = Req_Base;
		class Req_Async; using Request_Async = Req_Async;
		class Req_Box;   using Request_Box   = Req_Box;


		/*
			Request communicator that calls an AsyncQuery delegate.
		*/
		class Req_Async : public Req_Base
		{
		public:
			Req_Async(std::shared_ptr<AsyncQuery> p)                                 : Req_Base(p),           _delegate(p) {}
			Req_Async(std::shared_ptr<AsyncQuery> p, const Req_Base &shareSocket)    : Req_Base(shareSocket), _delegate(p) {}
			~Req_Async();

			/*
				Initiate a request.
					May fail, throwing nng::exception.
			*/
			QueryID request(nng::msg &&msg);


			/*
				Stats implementation
			*/
			MsgStats msgStats() const noexcept final;


		protected:
			std::shared_ptr<AsyncQuery> _delegate;

			enum ACTION_STATE
			{
				IDLE = 0,
				SEND = 1,
				RECV = 2,
			};

			struct Action;
			friend struct Action;
			mutable std::mutex          mtx;
			std::unordered_set<Action*> active;
			std::deque<Action*>         idle;
		};


		/*
			Non-blocking client socket for requests.
				Supports multiple pending requests.
				Requests are handled with std::future.
		*/
		class Req_Box : public Req_Async
		{
		public:
			explicit Req_Box();
			Req_Box(const Req_Base &o);
			~Req_Box();

			/*
				Send a request to the server.
			*/
			std::future<nng::msg> request(nng::msg &&msg);


		protected:
			class Delegate;
		};
	}
}
