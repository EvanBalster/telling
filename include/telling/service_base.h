#pragma once

#include <optional>

#include "service_registration.h"

#include "service_reply.h"
#include "service_pull.h"
#include "service_publish.h"



namespace telling
{
	/*
		Base class for Services.
	*/
	class Service_Base
	{
	public:
		// In-process URI.
		const std::string         uri;
		HostAddress::Base         inProcAddress() const noexcept    {return HostAddress::Base::InProc(uri);}

		// Primary registration.  Additional registrations under different URIs are allowed.
		std::optional<Registration> registration;


	public:
		Service_Base(std::string _uri, std::string_view serverID = DefaultServerID());
		virtual ~Service_Base();

		void registerURI(std::string_view serverID);


		/*
			Services typically register with a server rather than listening.
				It is unusual but possible for a service to dial a listening client.
				When a client is connected directly to a service, no URI routing occurs.
		*/
		void dial         (const HostAddress::Base &base)             {Dial      (base, replier(), publisher(), puller());}
		void listen       (const HostAddress::Base &base)             {Listen    (base, replier(), publisher(), puller());}
		void disconnect   (const HostAddress::Base &base) noexcept    {Disconnect(base, replier(), publisher(), puller());}
		void disconnectAll()                              noexcept    {DisconnectAll   (replier(), publisher(), puller());}
		void close        ()                              noexcept    {Close           (replier(), publisher(), puller());}


		/*
			Access individual communicators.
		*/
		virtual Reply_Base   *replier()   noexcept = 0;
		virtual Publish_Base *publisher() noexcept = 0;
		virtual Pull_Base    *puller()    noexcept = 0;

		/*
			Publish a message to a topic (URI).
		*/
		virtual void publish(nng::msg &&report) = 0;
	};



	/*
		Bare-bones base class for service handlers.
	*/
	class ServiceHandler_Base :
		public AsyncReply,
		public AsyncPublish,
		public AsyncPull,
		public Socket::PipeEventHandler
	{
	public:
		using QueryID = telling::QueryID;

	public:
		~ServiceHandler_Base() override {}
	};


	/*
		Basic service I/O handler.
			Includes an outbox queue for published messages.
	*/
	class ServiceHandler : public ServiceHandler_Base
	{
	public:
		using AsyncError = telling::AsyncError;
		using Replying   = telling::Replying;
		using Publishing = telling::Publishing;
		using Pulling    = telling::Pulling;


	public:
		~ServiceHandler() override {}


	protected:
		// Receive a pull message.
		// void async_recv (Pulling, nng::msg &&request) -- REQUIRED
		virtual void async_error(Pulling, AsyncError)       {}

		// Request / Reply processing.
		// void async_recv (Replying, nng::msg &&request) -- REQUIRED
		void async_prep (Replying, nng::msg &)   override    {}
		void async_sent (Replying)               override    {}
		void async_error(Replying, AsyncError)   override    {}

		// Publishing errors (optional)
		void async_error(Publishing, AsyncError) override    {}

		// Optionally receive pipe events from the various sockets.
		void pipeEvent(Socket*, nng::pipe_view, nng::pipe_ev) override    {}


	private:
		SendQueueMtx publishQueue;

		// AsyncSend (Publish) impl.
		void async_prep (Publishing, nng::msg &msg) override;
		void async_sent (Publishing)                override;
	};
}