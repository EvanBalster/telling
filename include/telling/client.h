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
		A non-blocking client which is checked like a mailbox.
	*/
	class Client_Box : public Client_Base
	{
	public:
		Client_Box();
		~Client_Box();


		/*
			Push a message to the server. Throws nng::exception on failure.
		*/
		void push(nng::msg &&msg) final                  {_pusher.push(std::move(msg));}


		/*
			Create a request. Throws nng::exception on failure.
		*/
		std::future<nng::msg> request(nng::msg &&msg)    {return _requester.request(std::move(msg));}


		/*
			Check for messages from subscribed topics.
				No messages will be received unless you subscribe() to something!
		*/
		bool consume    (nng::msg &msg)                  {return _subscriber.consume(msg);}


		// Access communicators.
		Request_Base   *requester()  noexcept final    {return &_requester;}
		Subscribe_Base *subscriber() noexcept final    {return &_subscriber;}
		Push_Base      *pusher()     noexcept final    {return &_pusher;}


	protected:
		Request_Box   _requester;
		Subscribe_Box _subscriber;
		Push_Box      _pusher;
	};


	/*
		A client which receives messages using asynchronous events.
	*/
	class Client : public Client_Base
	{
	public:
		/*
			Asynchronous events are delivered to a delegate object.
		*/
		class Handler :
			public AsyncRequest,
			public AsyncSubscribe,
			public AsyncPush
		{
		public:
			using QueryID   = telling::QueryID;


		protected:
			virtual ~Handler() {}

			// Receive a subscribe message.
			// There is no method for replying.
			virtual void subscribe_recv (nng::msg &&bulletin) = 0;
			virtual void subscribe_error(AsyncError)         {}

			// Receive a reply to some earlier request.
			virtual void reply_recv   (QueryID id, nng::msg &&reply) = 0;

			// Request processing status (optional).
			// request_error may be also be triggered if there is some error sending a request.
			virtual void request_prep (QueryID id, nng::msg &request)    {}
			virtual void request_sent (QueryID id)                       {}
			virtual void request_error(QueryID id, AsyncError)           {}

			// Push outbox status (optional)
			virtual void push_sent ()              {}
			virtual void push_error(AsyncError)    {}

			// Optionally receive pipe events from the various sockets.
			virtual void pipeEvent(Socket*, nng::pipe_view, nng::pipe_ev) {}


		private:
			SendQueueMtx pushQueue;

			// AsyncQuery impl.
			void async_prep (Requesting req, nng::msg &msg)     final    {this->request_prep(req.id, msg);}
			void async_sent (Requesting req)                    final    {this->request_sent(req.id);}
			void async_recv (Requesting req, nng::msg &&reply)  final    {this->reply_recv(req.id, std::move(reply));}
			void async_error(Requesting req, AsyncError status) final    {request_error(req.id, status);}

			// AsyncRecv (Subscribe) impl.
			void async_recv (Subscribing, nng::msg &&msg   ) final    {this->subscribe_recv(std::move(msg));}
			void async_error(Subscribing, AsyncError status) final    {this->subscribe_error(status);}

			// AsyncSend (Push) impl.
			void async_prep (Pushing, nng::msg &msg)     final;
			void async_sent (Pushing)                    final;
			void async_error(Pushing, AsyncError status) final    {this->push_error(status);}
		};


	public:
		Client(std::weak_ptr<Handler> handler);
		~Client();

		/*
			Push a message to the server. Throws nng::exception on failure.
		*/
		void push(nng::msg &&msg) final                   {_pusher.push(std::move(msg));}

		/*
			Create a request. Throws nng::exception on failure.
				Handler will eventually get reply_recv or request_error.
		*/
		void request(nng::msg &&msg)                      {_requester.request(std::move(msg));}

		/*
			Use subscribe(string topic) to set up subscriptions.
				Handler may then get subscribe_recv or subscribe_error.
		*/


		// Access communicators.
		Request_Base   *requester()  noexcept final    {return &_requester;}
		Subscribe_Base *subscriber() noexcept final    {return &_subscriber;}
		Push_Base      *pusher()     noexcept final    {return &_pusher;}


	protected:
		//std::weak_ptr<Handler> handler;
		Request    _requester;
		Subscribe  _subscriber;
		Push       _pusher;
	};
}