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


Service_Async::Service_Async(std::weak_ptr<ServiceHandler_Base> _handler,
	std::string _uri, std::string_view serverID)
	: Service_Base(_uri, serverID),
	//handler(std::move(_handler)),
	_replier  (_handler),
	_puller   (_handler),
	_publisher(_handler)
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



Directive ServiceHandler::asyncSend_msg(nng::msg &&msg)
{
	if (publishQueue.produce(std::move(msg))) return CONTINUE;
	return std::move(msg);
}
Directive ServiceHandler::asyncSend_sent()
{
	auto direct = this->publish_sent();

	if (!direct.msg()) switch (direct)
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



Directive Reactor::_handle(QueryID qid, nng::msg &&msg)
{
	Directive result = DECLINE;

	MsgView::Request request;

	Method method = MethodCode::None;

	try
	{
		// Parse.
		request = MsgView::Request(msg);
		method = request.method();
	}
	catch (MsgException e)
	{
		result = e.writeReply("Service Event Handler");
	}

	if (!result.msg())
	{
		std::lock_guard g(reactor_mtx);

		switch (method.code)
		{
		default:
		case MethodCode::CONNECT: result = DECLINE; break;
		case MethodCode::None:    result = DECLINE; break;

			// Safe
		case MethodCode::GET:    result = qid ? this->recv_get    (qid, request, std::move(msg)) : Directive(DECLINE); break;
		case MethodCode::HEAD:   result = qid ? this->recv_head   (qid, request, std::move(msg)) : Directive(DECLINE); break;
		case MethodCode::OPTIONS:result = qid ? this->recv_options(qid, request, std::move(msg)) : Directive(DECLINE); break;
		case MethodCode::TRACE:  result = qid ? this->recv_trace  (qid, request, std::move(msg)) : Directive(DECLINE); break;

			// Idempotent
		case MethodCode::PUT:    result = this->recv_put    (qid, request, std::move(msg)); break;
		case MethodCode::DELETE: result = this->recv_delete (qid, request, std::move(msg)); break;

			// Other
		case MethodCode::PATCH:  result = this->recv_patch  (qid, request, std::move(msg)); break;
		case MethodCode::POST:   result = this->recv_post   (qid, request, std::move(msg)); break;

			// Nonstandard
		case MethodCode::Unknown:result = this->recv_UNKNOWN(qid, request, std::move(msg)); break;
		}
	}

	return result;
}

Directive Reactor::pull_recv(nng::msg &&msg)
{
	auto directive = _handle(0, std::move(msg));

	return directive;
}
Directive Reactor::request_recv(QueryID id, nng::msg &&msg)
{
	auto directive = _handle(id, std::move(msg));

	/*
		TODO:  any more automatic headers?
	*/
	if (!directive.msg() && directive != TERMINATE)
	{
		MsgWriter reply;
		switch (directive)
		{
		case AUTO:
		case CONTINUE:
			reply.startReply(StatusCode::NoContent);
			break;

		case DECLINE:
		default:
			reply.startReply(StatusCode::MethodNotAllowed);
			break;
		}
		reply.writeHeader_Allowed(this->allowed());
		directive = reply.release();
	}

	return directive;
}


Directive Reactor::recv_options(QueryID id, const MsgView::Request &req, nng::msg &&msg)
{
	auto reply = WriteReply();
	reply.writeHeader_Allowed(this->allowed());
	return reply.release();
}