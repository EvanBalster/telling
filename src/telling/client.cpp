#include <telling/client.h>


using namespace telling;


Client_Box::Client_Box()
{
}
Client_Box::~Client_Box()
{
	close();
}


AsyncOp::SendDirective Client_Async::Handler::asyncSend_msg(nng::msg &&msg)
{
	if (pushQueue.produce(std::move(msg))) return CONTINUE;
	return SendDirective(std::move(msg));
}
AsyncOp::SendDirective Client_Async::Handler::asyncSend_sent()
{
	auto direct = this->push_sent();

	if (!direct.sendMsg) switch (direct.directive)
	{
	case DECLINE: case TERMINATE:
		// Interrupt sending
		break;
	case AUTO: case CONTINUE: default:
		nng::msg next;
		if (pushQueue.consume(next)) direct = std::move(next);
		else                         direct = DECLINE;
		break;
	}
	return direct;
}


Client_Async::Client_Async(std::weak_ptr<Handler> _handler) :
	//handler(std::move(_handler)),
	_requester (_handler),
	_subscriber(_handler),
	_pusher    (_handler)
{
}
Client_Async::~Client_Async()
{
	close();
}