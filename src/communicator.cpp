#include <telling/socket.h>


using namespace telling;


Communicator::Communicator(
	ROLE    _role,
	PATTERN _pattern) :
	role(_role),
	pattern(_pattern),
	protocol(Protocol::Choose(role, pattern)),
	_socket(std::make_shared<Socket>(_role, _pattern))
{
}

Communicator::Communicator(std::shared_ptr<Socket> socket) :
	role   (socket ? socket->role    : Role   ::NO_ROLE),
	pattern(socket ? socket->pattern : Pattern::NO_PATTERN),
	protocol(Protocol::Choose(role, pattern)),
	_socket(socket)
{
}

Communicator::~Communicator()
{
	_socket = nullptr;
}
