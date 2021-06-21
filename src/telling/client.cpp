#include <telling/client.h>


using namespace telling;


Client_Box::Client_Box()
{
}
Client_Box::~Client_Box()
{
	close();
}


void Client::Handler::async_prep(Pushing push, nng::msg &msg)
{
	if (pushQueue.produce(std::move(msg))) return;
	push.send(std::move(msg));
}
void Client::Handler::async_sent(Pushing push)
{
	this->push_sent();

	nng::msg next;
	if (pushQueue.consume(next)) push.send(std::move(next));
}


Client::Client(std::weak_ptr<Handler> _handler) :
	//handler(std::move(_handler)),
	_requester (_handler),
	_subscriber(_handler),
	_pusher    (_handler)
{
}
Client::~Client()
{
	close();
}