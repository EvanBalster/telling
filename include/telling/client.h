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
		virtual client::Request_Base   *requester()  noexcept = 0;
		virtual client::Subscribe_Base *subscriber() noexcept = 0;
		virtual client::Push_Base      *pusher()     noexcept = 0;


		/*
			Manage subscriptions.
		*/
		void subscribe  (std::string_view topic)     {auto s=subscriber(); if (s) s->  subscribe(topic);}
		void unsubscribe(std::string_view topic)     {auto s=subscriber(); if (s) s->unsubscribe(topic);}
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
		void push(nng::msg &&msg)                        {_pusher.push(std::move(msg));}


		/*
			Create a request. Throws nng::exception on failure.
		*/
		std::future<nng::msg> request(nng::msg &&msg)    {return _requester.request(std::move(msg));}


		/*
			Check for messages from subscribed topics.
				No messages will be received unless you subscribe() to something!
		*/
		bool consume    (nng::msg &msg)                   {return _subscriber.consume(msg);}


		// Access communicators.
		client::Request_Base   *requester()  noexcept final    {return &_requester;}
		client::Subscribe_Base *subscriber() noexcept final    {return &_subscriber;}
		client::Push_Base      *pusher()     noexcept final    {return &_pusher;}


	protected:
		client::Request_Box   _requester;
		client::Subscribe_Box _subscriber;
		client::Push_Box      _pusher;
	};


	/*
		A client which receives messages using asynchronous events.
	*/
	class Client_Async : public Client_Base
	{
	public:
		using SendDirective = AsyncOp::SendDirective;
		using Directive     = AsyncOp::Directive;


		/*
			Asynchronous events are delivered to a delegate object.
		*/
		class Handler :
			public AsyncSend,
			public AsyncRecv,
			public AsyncQuery
		{
		public:
			virtual ~Handler() {}

			using SendDirective = AsyncOp::SendDirective;
			using Directive     = AsyncOp::Directive;


		protected:
			// Receive a subscribe message.
			// There is no method for replying.
			virtual Directive     subscribe_recv (nng::msg &&bulletin) = 0;
			virtual Directive     subscribe_error(nng::error)         {return AUTO;}

			// Receive a reply to some earlier request.
			virtual Directive     reply_recv   (QueryID id, nng::msg &&reply) = 0;

			// Request processing status (optional).
			// request_error may be also be triggered if there is some error sending a request.
			virtual Directive     request_made (QueryID id, const nng::msg &request)    {return CONTINUE;}
			virtual Directive     request_sent (QueryID id)                             {return CONTINUE;}
			virtual Directive     request_error(QueryID id, nng::error)                 {return AUTO;}

			// Push outbox status (optional)
			virtual SendDirective push_sent ()              {return CONTINUE;}
			virtual SendDirective push_error(nng::error)    {return AUTO;}

			// Optionally receive pipe events from the various sockets.
			virtual void pipeEvent(Socket*, nng::pipe_view, nng::pipe_ev) {}


		private:
			SendQueueMtx pushQueue;

			// AsyncQuery impl.
			Directive asyncQuery_made (QueryID qid, const nng::msg &msg) final    {return this->request_made(qid, msg);}
			Directive asyncQuery_sent (QueryID qid)                      final    {return this->request_sent(qid);}
			Directive asyncQuery_done (QueryID qid, nng::msg &&reply)    final    {return this->reply_recv(qid, std::move(reply));}
			Directive asyncQuery_error(QueryID qid, nng::error status)   final    {return request_error(qid, status);}

			// AsyncRecv (Pull) impl.
			Directive asyncRecv_msg  (nng::msg &&msg   ) final    {return this->subscribe_recv(std::move(msg));}
			Directive asyncRecv_error(nng::error status) final    {return this->subscribe_error(status);}

			// AsyncSend (Publish) impl.
			SendDirective asyncSend_msg  (nng::msg &&msg)    final;
			SendDirective asyncSend_sent ()                  final;
			SendDirective asyncSend_error(nng::error status) final    {return this->push_error(status);}
		};


	public:
		Client_Async(std::shared_ptr<Handler> handler);
		~Client_Async();

		/*
			Push a message to the server. Throws nng::exception on failure.
		*/
		void push(nng::msg &&msg)                         {_pusher.push(std::move(msg));}

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
		client::Request_Base   *requester()  noexcept final    {return &_requester;}
		client::Subscribe_Base *subscriber() noexcept final    {return &_subscriber;}
		client::Push_Base      *pusher()     noexcept final    {return &_pusher;}


	protected:
		//std::shared_ptr<Handler> handler;
		client::Request_Async    _requester;
		client::Subscribe_Async  _subscriber;
		client::Push_Async       _pusher;
	};
}