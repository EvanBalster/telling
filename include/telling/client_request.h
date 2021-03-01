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
		using Req_Base = Communicator::Pattern_Base<Role::CLIENT, Pattern::REQ_REP>;


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
			Req_Async(std::shared_ptr<AsyncQuery> p)                       : _delegate(p), Req_Base() {}
			Req_Async(std::shared_ptr<AsyncQuery> p, const Req_Base &o)    : _delegate(p), Req_Base(o) {}
			~Req_Async();

			/*
				Initiate a request.
					May fail, throwing nng::exception.
			*/
			QueryID makeRequest(nng::msg &&msg);


			/*
				Get statistics
			*/
			size_t countUnsent();
			size_t countSentAwaitingReply();


		protected:
			std::shared_ptr<AsyncQuery> _delegate;

			enum ACTION_STATE
			{
				IDLE = 0,
				SEND = 1,
				RECV = 2,
			};

			struct Action
			{
				friend class Request;

				Req_Async* const       request;
				nng::aio               aio;
				nng::ctx               ctx;
				ACTION_STATE           state;
				//std::promise<nng::msg> promise;

				QueryID queryID() const noexcept    {return ctx.get().id;}

				static void _callback(void*);
			};

			friend struct Action;
			std::mutex                  mtx;
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
