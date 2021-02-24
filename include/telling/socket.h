#pragma once


#include <memory>
#include <vector>
#include <unordered_map>

#include <nngpp/nngpp.h>
#include "host_address.h"


namespace telling
{
	/*
		Utility class encapsulating an NNG socket with a role in a pattern.
			Includes any listeners and dialers.
	*/
	class Socket
	{
	public:
		class Subscribe;
		class Push;
		class Request;

		enum VARIANT
		{
			STANDARD = 0,
			RAW      = 1,
		};


	public:
		Socket(
			ROLE    _role,
			PATTERN _pattern,
			VARIANT _variant = STANDARD);

		~Socket();


		/*
			Close the socket and all dialers/listeners.
				This will interrupt any operations in progress.
		*/
		void close() noexcept;


		nng::socket_view socketView() const    {return _socket;}

		nng::ctx         make_ctx()   const    {return nng::make_ctx(_socket);}

		/*
			Socket status.
				isOpen      : the socket has not been closed or std::move'd.
				isReady     : at least one listener or dialer is active
				isConnected : at least one peer is connected to the socket
		*/
		bool isOpen     () const noexcept    {return bool(_socket);}
		bool isReady    () const noexcept    {return _connectors.size();}
		bool isConnected() const noexcept    {return _pipe_count > 0;}

		// Count live connections.
		uint32_t connectionCount() const noexcept    {return _pipe_count;}

		/*
			Dial/listen at the given address.
		*/
		void dial      (const std::string &uri);
		void listen    (const std::string &uri);
		void disconnect(const std::string &uri) noexcept;

		void dial      (HostAddress    address)             {std::string uri = address;       dial      (uri);}
		void listen    (HostAddress    address)             {std::string uri = address;       listen    (uri);}
		void disconnect(HostAddress    address) noexcept    {std::string uri = address;       disconnect(uri);}

		void dial      (HostAddress::Base base)             {std::string uri = base[pattern]; dial      (uri);}
		void listen    (HostAddress::Base base)             {std::string uri = base[pattern]; listen    (uri);}
		void disconnect(HostAddress::Base base) noexcept    {std::string uri = base[pattern]; disconnect(uri);}

		/*
			Close any listeners or dialers associated with the socket.
		*/
		void disconnectAll() noexcept;


	public:
		const ROLE     role;
		const PATTERN  pattern;
		const PROTOCOL protocol;


	protected:
		nng::socket   _socket;
		uint32_t      _pipe_count = 0;

		// Asynchronous pipe event
		virtual void pipeEvent(nng::pipe_view pipe, nng::pipe_ev event) {}

		// Can be a dialer or listener.
		struct ListenerOrDialer
		{
		public:
			ListenerOrDialer(nng::dialer   &&dialer);
			ListenerOrDialer(nng::listener &&listener);
			~ListenerOrDialer();

			explicit operator bool() const noexcept;

			nng::dialer_view   dialer  () const noexcept;
			nng::listener_view listener() const noexcept;

			uint32_t           id() const noexcept    {return handle.id;}


		private:
			bool isListener;
			nng_dialer handle;

			static_assert(sizeof(nng::dialer  ) == sizeof(nng_dialer));
			static_assert(sizeof(nng::listener) == sizeof(nng_dialer));
		};

		// TODO optimize later
		std::unordered_map<std::string, ListenerOrDialer> _connectors;


	private:
		static void _pipeCallback(nng_pipe, nng_pipe_ev, void*);
	};


	/*
		Base class which holds a single valid Socket of the given type.
	*/
	class Communicator : public UsingPatternEnums
	{
	public:
		~Communicator();


		/*
			Permanently close the communicator's shared socket.
		*/
		void close() const noexcept    {if (_socket) _socket->close();}


		/*
			Check socket's status.
		*/
		bool isOpen()      const noexcept    {return bool(_socket) && _socket->isOpen();}
		bool isReady()     const noexcept    {return bool(_socket) && _socket->isReady();}
		bool isConnected() const noexcept    {return bool(_socket) && _socket->isConnected();}

		// Count live connections.
		uint32_t connectionCount() const noexcept    {return bool(_socket) ? _socket->connectionCount() : 0;}


		/*
			Access the Communicator's socket.
				This can be shared with other Communicators via open().
		*/
		const std::shared_ptr<Socket> socket() const    {return _socket;}


		/*
			Proxy methods for Socket
		*/
		nng::socket_view socketView() const              {return _socket->socketView();}

		nng::ctx         make_ctx()   const              {return _socket->make_ctx();}

		void dial      (const std::string &uri) const             {_socket->dial      (uri);}
		void listen    (const std::string &uri) const             {_socket->listen    (uri);}
		void disconnect(const std::string &uri) const noexcept    {_socket->disconnect(uri);}

		void dial      (HostAddress    address) const             {_socket->dial      (address);}
		void listen    (HostAddress    address) const             {_socket->listen    (address);}
		void disconnect(HostAddress    address) const noexcept    {_socket->disconnect(address);}

		void dial      (HostAddress::Base base) const             {_socket->dial      (base);}
		void listen    (HostAddress::Base base) const             {_socket->listen    (base);}
		void disconnect(HostAddress::Base base) const noexcept    {_socket->disconnect(base);}

		void disconnectAll() const noexcept                       {_socket->disconnectAll();}



	public:
		const ROLE     role;
		const PATTERN  pattern;
		const PROTOCOL protocol;


	protected:
		std::shared_ptr<Socket> _socket;

		/*
			Create a Communicator holding a new socket.
		*/
		Communicator(
			ROLE    _role,
			PATTERN _pattern);

		/*
			Create a communicator based on the given Socket.
				Throws nng::exception if the socket is null.
		*/
		Communicator(std::shared_ptr<Socket> socket);

		/*
			Create a Communicator sharing another communicator's socket.
		*/
		Communicator(const Communicator& ) = default;

		/*
			Can't assign, 
		*/
		Communicator  (      Communicator&&) = delete;
		void operator=(const Communicator& ) = delete;
		void operator=(      Communicator&&) = delete;
	};


	namespace detail
	{
		template<typename... Args>
		void pass(Args...) {}
	}

	template<class... Args>
	void Each_Close(Args&... args) noexcept
		{detail::pass((args.close(), 0) ...);}

	template<class... Args>
	void Each_DisconnectAll(Args&... args) noexcept
		{detail::pass((args.disconnectAll(), 0) ...);}

	template<class... Args>
	void Each_Disconnect(const HostAddress::Base &base, Args&... args) noexcept
		{detail::pass((args.disconnect(base), 0) ...);}

	template<class... Args>
	void Each_Dial(const HostAddress::Base &base, Args&... args)
	{
		try                          {detail::pass((args.dial      (base), 0) ...);}
		catch (nng::exception error) {detail::pass((args.disconnect(base), 0) ...); throw error;}
	}

	template<class... Args>
	void Each_Listen(const HostAddress::Base &base, Args&... args)
	{
		try                          {detail::pass((args.listen    (base), 0) ...);}
		catch (nng::exception error) {detail::pass((args.disconnect(base), 0) ...); throw error;}
	}
}