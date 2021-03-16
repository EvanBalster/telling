#pragma once


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
		virtual service::Reply_Base   *replier()   noexcept = 0;
		virtual service::Publish_Base *publisher() noexcept = 0;
		virtual service::Pull_Base    *puller()    noexcept = 0;
	};



	/*
		Bare-bones base class for service handlers.
	*/
	class ServiceHandler_Base :
		public AsyncSend,
		public AsyncRecv,
		public AsyncRespond
	{
	public:
		virtual ~ServiceHandler_Base() {}

		using SendDirective = AsyncOp::SendDirective;
		using Directive     = AsyncOp::Directive;
	};


	/*
		Basic service I/O handler.
			Includes an outbox queue for published messages.
	*/
	class ServiceHandler : public ServiceHandler_Base
	{
	public:
		virtual ~ServiceHandler() {}

		using SendDirective = AsyncOp::SendDirective;
		using Directive     = AsyncOp::Directive;


	protected:
		// Receive a pull message.
		// There is no method for replying.
		virtual Directive     pull_recv (nng::msg &&request) = 0;
		virtual Directive     pull_error(nng::error)         {}

		// Receive a request.
		// May respond immediately (return a msg) or later (via respondTo).
		virtual SendDirective request_recv(QueryID id, nng::msg &&request) = 0;

		// Reply processing status (optional).
		// reply_error may be also be triggered if there is some error receiving a request.
		virtual void          reply_sent (QueryID id)                {}
		virtual Directive     reply_error(QueryID id, nng::error)    {return AsyncOp::TERMINATE;}

		// Publish outbox status (optional)
		virtual SendDirective publish_sent ()              {}
		virtual SendDirective publish_error(nng::error)    {return AsyncOp::TERMINATE;}

		// Optionally receive pipe events from the various sockets.
		virtual void pipeEvent(Socket*, nng::pipe_view, nng::pipe_ev) {}


	private:
		SendQueueMtx publishQueue;

		// AsyncRespond impl.
		SendDirective asyncRespond_recv (QueryID qid, nng::msg &&m)    final    {return this->request_recv(qid, std::move(m));}
		void          asyncRespond_done (QueryID qid)                  final    {this->reply_sent(qid);}
		Directive     asyncRespond_error(QueryID qid, nng::error e)    final    {return this->reply_error(qid, e);}

		// AsyncRecv (Pull) impl.
		Directive asyncRecv_msg  (nng::msg &&msg   ) final    {return this->pull_recv(std::move(msg));}
		Directive asyncRecv_error(nng::error status) final    {return this->pull_error(status);}

		// AsyncSend (Publish) impl.
		SendDirective asyncSend_msg  (nng::msg &&msg)    final;
		SendDirective asyncSend_sent ()                  final;
		SendDirective asyncSend_error(nng::error status) final    {return this->publish_error(status);}
	};
}