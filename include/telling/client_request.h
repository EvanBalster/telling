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
		/*
			Non-blocking client socket for requests.
				Supports multiple pending requests.
				Requests are handled with std::future.
		*/
		class Request : public Communicator
		{
		public:
			explicit Request()           : Communicator(CLIENT, REQ_REP) {}
			Request(const Request &o)    : Communicator(o) {}
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
