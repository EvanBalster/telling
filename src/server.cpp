#include <iostream>

#include <telling/server.h>


using namespace telling;


namespace telling
{
	namespace server_detail
	{
		class NulStreambuf : public std::streambuf
		{
			char                dummyBuffer[ 64 ];
		protected:
			virtual int         overflow( int c ) 
			{
				setp( dummyBuffer, dummyBuffer + sizeof( dummyBuffer ) );
				return (c == traits_type::eof()) ? '\0' : c;
			}
		};
		class NulOStream : private NulStreambuf, public std::ostream
		{
		public:
			NulOStream() : std::ostream( this ) {}
			NulStreambuf* rdbuf() const { return (NulStreambuf*) this; }
		};

		static NulOStream nullOutput;
	}
}


Server::Server(std::ostream *_log, std::string_view _id) :
	ID(_id),
	address_services(HostAddress::Base::InProc(ID)),
	address_internal(HostAddress::Base::InProc(ID + "/internal")),
	log(_log ? *_log : server_detail::nullOutput)
{

	//device_aio_start(outer_pub, inner_sub, aio);
}

Server::~Server()
{

}

void Server::open(const HostAddress::Base &base)
{
	Listen(base, reply.hostSocket(), publish.hostSocket(), pull.hostSocket());
}
void Server::close(const HostAddress::Base &base)
{
	Disconnect(base, reply.hostSocket(), publish.hostSocket(), pull.hostSocket());
}
