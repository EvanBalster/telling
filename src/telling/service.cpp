#include <iostream>

#include <telling/service.h>
#include <telling/service_reactor.h>


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


Service::Service(std::string _uri, std::string_view serverID)
	: Service_Base(_uri, serverID)
	//handler(std::move(_handler)),
{
	listen(inProcAddress());
}
Service::~Service()
{
	close();
}

void Service::initialize(std::weak_ptr<ServiceHandler_Base> _handler)
{
	_replier  .socket()->setPipeHandler(_handler);
	_puller   .socket()->setPipeHandler(_handler);
	_publisher.socket()->setPipeHandler(_handler);
	_replier  .initialize(_handler);
	_puller   .initialize(_handler);
	_publisher.initialize(_handler);
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
