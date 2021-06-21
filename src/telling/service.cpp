#include <iostream>

#include <telling/service.h>

#include <telling/msg_writer.h>
#include <telling/msg_view.h>


using namespace telling;



Service_Base::Service_Base(std::string _uri, std::string_view serverID) :
	uri(_uri)
{
	if (serverID.length()) registerURI(serverID);
}

Service_Base::~Service_Base()
{
}

void Service_Base::registerURI(std::string_view serverID)
{
	if (registration)
		throw nng::exception(nng::error::busy, "Service Registration already in progress.");

	registration.emplace(uri, uri, serverID);
}


Service::Service(std::weak_ptr<ServiceHandler_Base> _handler,
	std::string _uri, std::string_view serverID)
	: Service_Base(_uri, serverID),
	//handler(std::move(_handler)),
	_replier  (_handler),
	_puller   (_handler),
	_publisher(_handler)
{
	listen(inProcAddress());
}
Service::~Service()
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



void ServiceHandler::async_prep(Publishing pub, nng::msg &msg)
{
	if (!publishQueue.produce(std::move(msg)))
		pub.send(std::move(msg));
}
void ServiceHandler::async_sent(Publishing pub)
{
	nng::msg next;
	if (publishQueue.consume(next)) pub.send(std::move(next));
}



void Reactor::_handle(Query query, nng::msg &&msg)
{
	MsgView::Request request;
	Method           method = MethodCode::None;
	try
	{
		// Parse.
		request = MsgView::Request(msg);
		method = request.method();
	}
	catch (MsgException e)
	{
		query.reply(e.writeReply("Service Event Handler"));
		return;
	}


	std::lock_guard g(reactor_mtx);

	nng::msg rep;

	switch (method.code)
	{
	default:
	case MethodCode::CONNECT: break;
	case MethodCode::None:    break;

		// Safe
	case MethodCode::GET:     this->async_get    (query, request, std::move(msg)); break;
	case MethodCode::HEAD:    this->async_head   (query, request, std::move(msg)); break;
	case MethodCode::OPTIONS: this->async_options(query, request, std::move(msg)); break;
	case MethodCode::TRACE:   this->async_trace  (query, request, std::move(msg)); break;

		// Idempotent
	case MethodCode::PUT:     this->async_put    (query, request, std::move(msg)); break;
	case MethodCode::DELETE:  this->async_delete (query, request, std::move(msg)); break;

		// Other
	case MethodCode::PATCH:   this->async_patch  (query, request, std::move(msg)); break;
	case MethodCode::POST:    this->async_post   (query, request, std::move(msg)); break;

		// Nonstandard
	case MethodCode::Unknown: this->async_UNKNOWN(query, request, std::move(msg)); break;
	}
}


void Reactor::async_trace(Query query, const MsgView::Request &req, nng::msg &&msg)
{
	auto reply = WriteReply(StatusCode::OK);
	reply.writeHeader("Content-Type", "http");
	return query.reply(reply.release());
}

void Reactor::async_options(Query query, const MsgView::Request &req, nng::msg &&msg)
{
	auto reply = WriteReply();
	reply.writeHeader_Allow(this->allowed());
	return query.reply(reply.release());
}

nng::msg Reactor::NotImplemented()
{
	auto reply = WriteReply(StatusCode::NotImplemented);
	reply.writeHeader_Allow(this->allowed());
	return reply.release();
}
