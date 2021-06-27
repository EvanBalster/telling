#pragma once

#include "client_push.h"
#include "client_request.h"
#include "client_subscribe.h"


namespace telling
{
	/*
		Base class for Clients which may support all three messaging patterns.
	*/
	class Client_Base
	{
	public:
		Client_Base()          {}
		virtual ~Client_Base() {}


		/*
			Clients must contact a server or service in order to begin communicating.
				Clients will most often dial into a server.
				Less commonly, they may dial a service directly, bypassing URI routing.
				It is also possible for clients to listen and servers/services to dial them.

			If dialed into multiple servers/services, Request and Push will fair-queue.
		*/
		void dial         (const HostAddress::Base &base)             {Dial      (base, requester(), subscriber(), pusher());}
		void listen       (const HostAddress::Base &base)             {Listen    (base, requester(), subscriber(), pusher());}
		void disconnect   (const HostAddress::Base &base) noexcept    {Disconnect(base, requester(), subscriber(), pusher());}
		void disconnectAll()                              noexcept    {DisconnectAll   (requester(), subscriber(), pusher());}
		void close        ()                              noexcept    {Close           (requester(), subscriber(), pusher());}


		/*
			Access individual communicators.
		*/
		virtual Request_Base   *requester()  noexcept = 0;
		virtual Subscribe_Base *subscriber() noexcept = 0;
		virtual Push_Base      *pusher()     noexcept = 0;


		/*
			Manage subscriptions.
		*/
		void subscribe  (std::string_view topic)     {auto s=subscriber(); if (s) s->  subscribe(topic);}
		void unsubscribe(std::string_view topic)     {auto s=subscriber(); if (s) s->unsubscribe(topic);}

		/*
			Push a request with no means of reply.
		*/
		virtual void push(nng::msg &&request) = 0;
	};


	/*
		Bare-bones base class for client handlers.
	*/
	class ClientHandler_Base :
		public AsyncRequest,
		public AsyncSubscribe,
		public AsyncPush,
		public Socket::PipeEventHandler
	{
	public:
		using QueryID = telling::QueryID;

	public:
		~ClientHandler_Base() override {}
	};


	/*
		Asynchronous events are delivered to a handler object.
	*/
	class ClientHandler :
		public ClientHandler_Base
	{
	public:
		using AsyncError  = telling::AsyncError;
		using Requesting  = telling::Requesting;
		using Subscribing = telling::Subscribing;
		using Pushing     = telling::Pushing;


	public:
		~ClientHandler() override {}


	protected:
		// Subscription processing
		// void async_recv (Subscribing, nng::msg &&report) -- REQUIRED
		void async_error(Subscribing, AsyncError)         {}

		// Request/reply processing
		void async_prep (Requesting req, nng::msg &) override    {}
		void async_sent (Requesting req)             override    {}
		// void async_recv (Requesting, nng::msg &&reply) -- REQUIRED
		void async_error(Requesting req, AsyncError) override    {}

		// Pushing errors (optional)
		void async_error(Pushing, AsyncError) override    {}

		// Optionally receive pipe events from the various sockets.
		void pipeEvent(Socket*, nng::pipe_view, nng::pipe_ev) override {}


	private:
		SendQueueMtx pushQueue;

		// AsyncSend (Push) impl.
		void async_prep (Pushing, nng::msg &msg)     final;
		void async_sent (Pushing)                    final;
	};
}