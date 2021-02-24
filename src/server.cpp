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
	reply  .hostSocket().listen(base);
	publish.hostSocket().listen(base);
	pull   .hostSocket().listen(base);
}
void Server::close(const HostAddress::Base &base)
{
	reply  .hostSocket().disconnect(base);
	publish.hostSocket().disconnect(base);
	pull   .hostSocket().disconnect(base);
}
