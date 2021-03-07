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




AsyncOp::SendDirective Service_Async::Handler::asyncSend_msg(nng::msg &&msg)
{
	if (publishQueue.produce(std::move(msg))) return CONTINUE;
	return SendDirective(std::move(msg));
}
AsyncOp::SendDirective Service_Async::Handler::asyncSend_sent()
{
	auto direct = this->publish_sent();
		
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


Service_Async::Service_Async(std::shared_ptr<Handler> _handler,
	std::string _uri, std::string_view serverID)
	: Service_Base(_uri, serverID),
	//handler(std::move(_handler)),
	_replier  (std::static_pointer_cast<AsyncRespond>(_handler)),
	_puller   (std::static_pointer_cast<AsyncRecv   >(_handler)),
	_publisher(std::static_pointer_cast<AsyncSend   >(_handler))
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