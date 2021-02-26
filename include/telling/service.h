#pragma once

#include "service_reply.h"
#include "service_pull.h"
#include "service_publish.h"


namespace telling
{
	/*
		A non-blocking service which is checked like a mailbox.
	*/
	class Service_Box
	{
	public:
		const std::string uri;

		HostAddress::Base address() const    {return HostAddress::Base::InProc(uri);}

		service::Reply       replier;
		service::Pull_Box    puller;
		service::Publish_Box publisher;


	public:
		Service_Box(std::string _uri, std::string_view serverID = std::string_view());
		~Service_Box();

		void serve(std::string_view serverID);

		// In some cases, the service registration may need to be re-published.
		void broadcastServiceRegistration();

		/*
			Services can use serverID instead of manually listening.
				It is unusual but possible for a service to dial a listening client.
				When a client is connected directly to a service, no URI routing occurs.
		*/
		void dial         (const HostAddress::Base &base)             {Each_Dial      (base, replier, publisher, puller);}
		void listen       (const HostAddress::Base &base)             {Each_Dial      (base, replier, publisher, puller);}
		void disconnect   (const HostAddress::Base &base) noexcept    {Each_Disconnect(base, replier, publisher, puller);}
		void disconnectAll()                              noexcept    {Each_DisconnectAll   (replier, publisher, puller);}
		void close        ()                              noexcept    {Each_Close           (replier, publisher, puller);}


		/*
			Push a message to the server. Throws nng::exception on failure.
		*/
		void publish(nng::msg &&msg)         {publisher.publish(std::move(msg));}


		/*
			Receive pushed messages.
		*/
		bool pull(nng::msg &msg)             {return puller.pull(msg);}


		/*
			Receive and reply to requests (one by one).
		*/
		bool receive(nng::msg  &request)     {return replier.receive(request);}
		void respond(nng::msg &&reply)       {replier.respond(std::move(reply));}

		/*
			Reply to all pending requests with a functor.
		*/
		template<class Fn>          void respond_all(Fn fn)           {replier.respond_all(fn);}
		template<class Fn, class A> void respond_all(Fn fn, A arg)    {replier.respond_all(fn, arg);}
	};
}