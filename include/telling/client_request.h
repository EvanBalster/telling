#pragma once


#include <deque>
#include <mutex>
#include <future>
#include <robin_hood.h>
#include <unordered_set>
#include "socket.h"


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
			Non-blocking client socket for requests.
				Supports multiple pending requests.
				Requests are handled with std::future.
		*/
		class Request : public Req_Base
		{
		public:
			explicit Request()           : Req_Base() {}
			Request(const Request &o)    : Req_Base(o) {}
			~Request();

			/*
				Send a request to the server.
			*/
			std::future<nng::msg> request(nng::msg &&msg);


			/*
				Get statistics
			*/
			size_t countUnsent();
			size_t countSentAwaitingReply();


		protected:
			enum ACTION_STATE
			{
				IDLE = 0,
				SEND = 1,
				RECV = 2,
			};

			struct Action
			{
				friend class Request;

				Request* const         request;
				nng::aio               aio;
				nng::ctx               ctx;
				ACTION_STATE           state;
				std::promise<nng::msg> promise;

				static void _callback(void*);
			};

			friend struct Action;
			std::mutex                              mtx;
			//robin_hood::unordered_flat_set<Action*> active;
			std::unordered_set<Action*>             active;
			std::deque<Action*>                     idle;
		};
	}
}
