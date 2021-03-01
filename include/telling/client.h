#pragma once

#include "client_push.h"
#include "client_request.h"
#include "client_subscribe.h"


namespace telling
{
	/*
		A non-blocking client which is checked like a mailbox.
	*/
	class Client_Box
	{
	public:
		client::Subscribe_Box subscriber;
		client::Push_Box      pusher;
		client::Request_Box   requester;


	public:
		Client_Box();
		~Client_Box();

		/*
			Clients must contact a server or service in order to begin communicating.
				It is unusual but possible for a client to listen for a dialing service.
				When a client is connected directly to a service, no URI routing occurs.
		*/
		void dial         (const HostAddress::Base &base)             {Each_Dial      (base, requester, subscriber, pusher);}
		void listen       (const HostAddress::Base &base)             {Each_Dial      (base, requester, subscriber, pusher);}
		void disconnect   (const HostAddress::Base &base) noexcept    {Each_Disconnect(base, requester, subscriber, pusher);}
		void disconnectAll()                              noexcept    {Each_DisconnectAll   (requester, subscriber, pusher);}
		void close        ()                              noexcept    {Each_Close           (requester, subscriber, pusher);}


		/*
			Push a message to the server. Throws nng::exception on failure.
		*/
		void push(nng::msg &&msg)                        {pusher.push(std::move(msg));}


		/*
			Create a request. Throws nng::exception on failure.
		*/
		std::future<nng::msg> request(nng::msg &&msg)    {return requester.request(std::move(msg));}


		/*
			Manage subscriptions.
		*/
		void subscribe  (std::string_view topic)         {return subscriber.subscribe(topic);}
		void unsubscribe(std::string_view topic)         {return subscriber.unsubscribe(topic);}

		/*
			Check for messages from subscribed topics.
		*/
		bool consume    (nng::msg &msg)                  {return subscriber.consume(msg);}
	};
}