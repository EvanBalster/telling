#include <telling/client.h>


using namespace telling;


Client_Box::Client_Box()
{
}
Client_Box::~Client_Box()
{
	close();
}


void ClientHandler::async_prep(Pushing push, nng::msg &msg)
{
	if (!pushQueue.produce(std::move(msg)))
		push.send(std::move(msg));
}
void ClientHandler::async_sent(Pushing push)
{
	nng::msg next;
	if (pushQueue.consume(next)) push.send(std::move(next));
}


Client::Client()
{
}
Client::~Client()
{
	close();
}

void Client::initialize(std::weak_ptr<ClientHandler_Base> _handler)
{
	_requester .initialize(_handler);
	_subscriber.initialize(_handler);
	_pusher    .initialize(_handler);
}