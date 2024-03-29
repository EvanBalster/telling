#pragma once


#include <deque>
#include <mutex>
#include <future>
#include <life_lock.hpp>
#include <unordered_set>
#include "socket.h"

#include "async_loop.h"


namespace telling
{
	// Type hierarchy
	using Request_Pattern = Communicator::Pattern_Base<Role::CLIENT, Pattern::REQ_REP>;
	class Request_Base; // inherits Request_Pattern
	class Request;      // inherits Request_Base
	class Request_Box;  // inherits Request

	// Tag delivered to callbacks
	using Requesting = TagQuery<Request>;

	// Base class for asynchronous I/O
	using AsyncReq     = AsyncQuery<Requesting>;
	using AsyncRequest = AsyncReq;



	// Base type for Request clients.
	class Request_Base : public Request_Pattern
	{
	public:
		Request_Base()                                      : Request_Pattern()           {}
		Request_Base(const Request_Pattern &shareSocket)    : Request_Pattern(shareSocket) {}
		~Request_Base() {}
			

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


	/*
		Request communicator that calls an AsyncRequest handler.
	*/
	class Request : public Request_Base
	{
	public:
		/*
			Construct with asynchronous I/O handler and optional socket-sharing.
		*/
		Request()                                                       : Request_Base() {}
		Request(std::weak_ptr<AsyncReq> p)                              : Request() {initialize(p);}
		Request(const Request_Pattern &shared)                          : Request_Base(shared) {}
		Request(const Request_Pattern &s, std::weak_ptr<AsyncReq> p)    : Request(s) {initialize(p);}
		~Request();

		/*
			Provide a handler for handling requests after construction.
				Throws nng::exception if a handler has already been installed.
		*/
		void initialize(std::weak_ptr<AsyncReq>);

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
		std::weak_ptr<AsyncReq> _handler;

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
	class Request_Box : public Request
	{
	public:
		explicit Request_Box();
		Request_Box(const Request_Base &o);
		~Request_Box();

		/*
			Send a request to the server.
		*/
		std::future<nng::msg> request(nng::msg &&msg);


	protected:
		class Delegate;
		void _init();
		std::shared_ptr<Delegate> _requestBox;
	};
}
