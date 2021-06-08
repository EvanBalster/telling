#define SOCKET_LOGGING 0

#include <iostream>
#include <iomanip>
#include <mutex>

#include <nngpp/socket.h>
#include <nngpp/protocol/req0.h>
#include <nngpp/protocol/rep0.h>
#include <nngpp/protocol/pub0.h>
#include <nngpp/protocol/sub0.h>
#include <nngpp/protocol/push0.h>
#include <nngpp/protocol/pull0.h>
#include <nngpp/protocol/pair1.h>

#include <telling/socket.h>


using namespace telling;


static void LogSocketEvent(const Socket &s, std::string_view event, std::string_view addr, uint32_t otherID = 0)
{
#if SOCKET_LOGGING
	static std::mutex logMutex;
	std::lock_guard<std::mutex> g(logMutex);

	std::cout << "\t\t\t\tSocket #" << std::setw(3) << s.socketView().get().id;
	std::cout << ' ' << std::setw(4) << Role::Name4c(s.role);
	std::cout << ' ' << std::setw(4) << Protocol::Name(s.protocol);
	std::cout << ' ' << std::setw(8) << event;
	
	if (addr.length() || otherID) std::cout << ": ";
	if (addr.length()) std::cout << addr;
	if (otherID) std::cout << '#' << otherID;
	std::cout << std::endl;
#endif
}


Socket::ListenerOrDialer::ListenerOrDialer(nng::dialer   &&dialer)
{
	new (reinterpret_cast<nng::dialer  *>(&handle)) nng::dialer  (std::move(dialer  ));
	isListener = false;
}
Socket::ListenerOrDialer::ListenerOrDialer(nng::listener &&listener)
{
	new (reinterpret_cast<nng::listener*>(&handle)) nng::listener(std::move(listener));
	isListener = true;
}
Socket::ListenerOrDialer::~ListenerOrDialer()
{
	if (isListener)
		reinterpret_cast<nng::listener&>(handle).~listener();
	else
		reinterpret_cast<nng::dialer  &>(handle).~dialer();
}

Socket::ListenerOrDialer::operator bool() const noexcept
{
	return handle.id != 0;
}

nng::dialer_view Socket::ListenerOrDialer::dialer() const noexcept
{
	if (isListener) return nng::dialer_view();
	return reinterpret_cast<const nng::dialer_view&>(handle);
}
nng::listener_view Socket::ListenerOrDialer::listener() const noexcept
{
	if (!isListener) return nng::listener_view();
	return reinterpret_cast<const nng::listener_view&>(handle);
}


Socket::Socket(
	std::weak_ptr<PipeEventHandler> pipe_handler,
	ROLE    _role,
	PATTERN _pattern,
	VARIANT _variant) :
	Socket(_role, _pattern, _variant)
{
	_pipe_handler = pipe_handler;
}

Socket::Socket(
	ROLE    _role,
	PATTERN _pattern,
	VARIANT _variant) :
	role   (_role),
	pattern(_pattern),
	protocol(Protocol::Choose(role, pattern))
{
	switch (_variant)
	{
	case STANDARD:
		switch (protocol)
		{
		case Protocol::PAIR: _socket = nng::pair::open(); break;
		case Protocol::REQ:  _socket = nng::req ::open(); break;
		case Protocol::REP:  _socket = nng::rep ::open(); break;
		case Protocol::SUB:  _socket = nng::sub ::open(); break;
		case Protocol::PUB:  _socket = nng::pub ::open(); break;
		case Protocol::PUSH: _socket = nng::push::open(); break;
		case Protocol::PULL: _socket = nng::pull::open(); break;
		default:
			throw nng::exception(nng::error::inval, "Socket: invalid role/pattern configuration");
		}
		break;

	case RAW:
		switch (protocol)
		{
		case Protocol::PAIR: _socket = nng::pair::open_raw(); break;
		case Protocol::REQ:  _socket = nng::req ::open_raw(); break;
		case Protocol::REP:  _socket = nng::rep ::open_raw(); break;
		case Protocol::SUB:  _socket = nng::sub ::open_raw(); break;
		case Protocol::PUB:  _socket = nng::pub ::open_raw(); break;
		case Protocol::PUSH: _socket = nng::push::open_raw(); break;
		case Protocol::PULL: _socket = nng::pull::open_raw(); break;
		default:
			throw nng::exception(nng::error::inval, "Socket: invalid RAW role/pattern configuration");
		}
		break;

	default:
		throw nng::exception(nng::error::inval, "Socket: invalid variant");
	}
	
	_socket.pipe_notify(nng::pipe_ev::add_pre , &_pipeCallback, (void*) this);
	_socket.pipe_notify(nng::pipe_ev::add_post, &_pipeCallback, (void*) this);
	_socket.pipe_notify(nng::pipe_ev::rem_post, &_pipeCallback, (void*) this);

	LogSocketEvent(*this, "OPEN", "");
}

void Socket::setPipeHandler(std::weak_ptr<PipeEventHandler> new_handler)
{
	std::lock_guard<std::mutex> g(_mtx);

	auto current = _pipe_handler.lock();
	if (current) throw nng::exception(nng::error::busy, "telling::Socket::setPipeHandler");
	else         _pipe_handler = new_handler;
}

Socket::~Socket()
{
	close();
}

void Socket::close() noexcept
{
	std::lock_guard g(_mtx);

	if (_socket)
	{
		LogSocketEvent(*this, "CLOSE", "");
		_connectors.clear();
		
		_socket = nng::socket();
	}
}


void Socket::_pipeCallback(nng_pipe _pipe, nng_pipe_ev _event, void *_self)
{
	// Cast...
	nng::pipe_view pipe  = _pipe;
	nng::pipe_ev   event = nng::pipe_ev(_event);
	auto           self  = static_cast<Socket*>(_self);

	// Maintain a count of pipes.
	switch (event)
	{
	case nng::pipe_ev::add_post:                        ++self->_pipe_count; break;
	case nng::pipe_ev::rem_post: if (self->_pipe_count) --self->_pipe_count; break;
	default: break;
	}

#if SOCKET_LOGGING
	switch (event)
	{
	case nng::pipe_ev::add_pre : LogSocketEvent(*self, "ADD_PRE", "pipe", _pipe.id); break;
	case nng::pipe_ev::add_post: LogSocketEvent(*self, "ADD_POST", "pipe", _pipe.id); break;
	case nng::pipe_ev::rem_post: LogSocketEvent(*self, "REM_POST", "pipe", _pipe.id); break;
	default:                     LogSocketEvent(*self, "?EVENT?", "pipe", _pipe.id); break;
	}
#endif
	
	//if (self->_pipe_count) LogSocketEvent(*self, "# PIPES", "", self->_pipe_count);
	//else                   LogSocketEvent(*self, "# PIPES", "ZERO", 0);

	// Pipe event handler
	auto pipeHandler = self->_pipe_handler.lock();
	if (pipeHandler) pipeHandler->pipeEvent(self, pipe, event);
}



void Socket::dial(const std::string &uri)
{
	std::lock_guard g(_mtx);
	if (!_socket) throw nng::exception(nng::error::closed, "The socket is not open.");

	auto p = _connectors.emplace(uri, nng::make_dialer  (_socket, uri.c_str(), nng::flag::nonblock));

	LogSocketEvent(*this, "DIAL", uri, p.first->second.id());
}
void Socket::listen(const std::string &uri)
{
	std::lock_guard g(_mtx);
	if (!_socket) throw nng::exception(nng::error::closed, "The socket is not open.");

	auto p = _connectors.emplace(uri, nng::make_listener(_socket, uri.c_str(), nng::flag::nonblock));

	LogSocketEvent(*this, "LISTEN", uri, p.first->second.id());
}

void Socket::disconnect(const std::string &uri) noexcept
{
	std::lock_guard g(_mtx);
#if SOCKET_LOGGING
	{
		auto p = _connectors.find(uri);
		LogSocketEvent(*this, "DISCONN", uri, p->second.id());
	}
#endif

	_connectors.erase(uri);
}

void Socket::disconnectAll() noexcept
{
	std::lock_guard g(_mtx);
	LogSocketEvent(*this, "DISCONN", "(ALL)");

	_connectors.clear();
}
