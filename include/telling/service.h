#pragma once

#include "service_base.h"


namespace telling
{
	/*
		A non-blocking service which is checked like a mailbox.
	*/
	class Service_Box : public Service_Base
	{
	public:
		Service_Box(std::string _uri, std::string_view serverID = DefaultServerID());
		~Service_Box() override;


		/*
			Publish a message to a topic (URI).
		*/
		void publish(nng::msg &&msg) final   {_publisher.publish(std::move(msg));}


		/*
			Receive pushed messages.
		*/
		bool pull(nng::msg &msg)             {return _puller.pull(msg);}


		/*
			Receive and reply to requests (one by one).
		*/
		bool receive(nng::msg  &request)     {return _replier.receive(request);}
		void respond(nng::msg &&reply)       {_replier.respond(std::move(reply));}

		/*
			Reply to all pending requests with a functor.
		*/
		template<class Fn>          void respond_all(Fn fn)           {_replier.respond_all(fn);}
		template<class Fn, class A> void respond_all(Fn fn, A arg)    {_replier.respond_all(fn, arg);}


		// Access communicators.
		Reply_Base   *replier()   noexcept final    {return &_replier;}
		Publish_Base *publisher() noexcept final    {return &_publisher;}
		Pull_Base    *puller()    noexcept final    {return &_puller;}


	protected:
		Reply_Box   _replier;
		Publish_Box _publisher;
		Pull_Box    _puller;
	};


	/*
		A service which receives and responds to messages using asynchronous events.
	*/
	class Service : public Service_Base
	{
	public:
		Service(std::string _uri, std::string_view serverID = DefaultServerID());
		~Service();

		Service(std::weak_ptr<ServiceHandler_Base> handler,
			std::string _uri, std::string_view serverID = DefaultServerID()) :
			Service(_uri, serverID)    {initialize(handler);}

		/*
			Initialize service with a handler.
				Only one handler is allowed at a time.
		*/
		void initialize(std::weak_ptr<ServiceHandler_Base> handler);

		/*
			Publish a message to a topic (URI).
		*/
		void publish(nng::msg &&report) final                   {_publisher.publish(std::move(report));}

		/*
			Respond to the query with the given ID.
		*/
		void respondTo(QueryID queryID, nng::msg &&reply)       {_replier.respondTo(queryID, std::move(reply));}


		// Access communicators.
		Reply_Base   *replier()   noexcept final    {return &_replier;}
		Publish_Base *publisher() noexcept final    {return &_publisher;}
		Pull_Base    *puller()    noexcept final    {return &_puller;}


	protected:
		//std::weak_ptr<Handler> handler;
		Reply     _replier;
		Pull      _puller;
		Publish   _publisher;
	};
}