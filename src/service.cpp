#include <iostream>

#include <telling/service.h>

#include <telling/msg_writer.h>
#include <telling/msg_view.h>


using namespace telling;



Service_Base::Service_Base(std::string _uri, std::string_view serverID) :
	uri(_uri)
{
	if (serverID.length()) enlist(serverID);
}

Service_Base::~Service_Base()
{
}

void Service_Base::enlist(std::string_view serverID)
{
	if (enlistment)
		throw nng::exception(nng::error::busy, "Service Enlistment already in progress.");

	enlistment.emplace(serverID, uri);
}


class Service_Async::Delegate :
	public AsyncSend,
	public AsyncRecv,
	public AsyncRespond
{
public:
	std::shared_ptr<Handler> handler;

	SendQueueMtx publishQueue;

public:
	Delegate(std::shared_ptr<Handler> _handler) :
		handler(std::move(_handler))
	{

	}
	~Delegate() override {}

	/*
		AsyncRespond interface (reply pattern)
	*/
	SendDirective asyncRespond_recv (QueryID qid, nng::msg &&msg)    override {return handler->request_recv(qid, std::move(msg));}
	void          asyncRespond_done (QueryID qid)                    override {handler->reply_sent(qid);}
	Directive     asyncRespond_error(QueryID qid, nng::error status) override {return handler->reply_error(qid, status);}

	/*
		AsyncRecv interface (pull pattern)
	*/
	Directive asyncRecv_msg  (nng::msg &&msg   ) override {return handler->pull_recv(std::move(msg));}
	Directive asyncRecv_error(nng::error status) override {return handler->pull_error(status);}

	/*
		AsyncSend interface (publish pattern)
	*/
	SendDirective asyncSend_msg(nng::msg &&msg) override
	{
		if (publishQueue.produce(std::move(msg))) return CONTINUE;
		return SendDirective(std::move(msg));
	}
	SendDirective asyncSend_sent() override
	{
		auto direct = handler->publish_sent();
		
		if (!direct.sendMsg) switch (direct.directive)
		{
		case DECLINE: case TERMINATE:
			// Interrupt sending
			break;
		case AUTO: case CONTINUE: default:
			nng::msg next;
			if (publishQueue.consume(next)) direct = std::move(next);
			else                            direct = DECLINE;
			break;
		}
		return direct;
	}
	SendDirective asyncSend_error(nng::error status) override
	{
		return handler->publish_error(status);
	}
};


Service_Async::Service_Async(std::shared_ptr<Handler> handler,
	std::string _uri, std::string_view serverID)
	: Service_Base(_uri, serverID),
	delegate(std::make_shared<Delegate>(handler)),
	replier  (delegate),
	puller   (delegate),
	publisher(delegate)
{
	listen(inProcAddress());
}
Service_Async::~Service_Async()
{
	close();
}


Service_Box::Service_Box(std::string _uri, std::string_view serverID)
	: Service_Base(_uri, serverID)
{
	listen(inProcAddress());
}
Service_Box::~Service_Box()
{
	close();
}