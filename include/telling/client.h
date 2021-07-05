#pragma once

#include "client_base.h"


namespace telling
{
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
		Client();
		~Client();

		/*
			Initialize client with a handler.
				Only one handler is allowed at a time.
		*/
		void initialize(std::weak_ptr<ClientHandler_Base> handler);
		Client(std::weak_ptr<ClientHandler_Base> handler)    : Client() {initialize(handler);}

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