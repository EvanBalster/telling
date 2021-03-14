#include <iostream>

#include <telling/server.h>


using namespace telling;


Server::Server(std::string _id, std::ostream *_log) :
	ID(_id),
	address_services(HostAddress::Base::InProc(ID)),
	address_internal(HostAddress::Base::InProc(ID + "/internal")),
	log(_log ? *_log : std::cout)
{

	//device_aio_start(outer_pub, inner_sub, aio);
}

Server::~Server()
{

}

void Server::open(const HostAddress::Base &base)
{
	Listen(base, reply, publish, pull);
}
void Server::close(const HostAddress::Base &base)
{
	Close(base, reply, publish, pull);
}
