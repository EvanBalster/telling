#pragma once


#include <algorithm>
#include <string>
#include <string_view>

#include <nngpp/nngpp.h>

#include "pattern.h"


namespace telling
{
	class Transport
	{
	public:
		enum TRANSPORT : int8_t
		{
			NO_TRANSPORT = -1,
			INPROC = 0,
			IPC    = 1,
			TCP    = 2,
			TRANSPORT_COUNT
		};
	};

	using TRANSPORT = Transport::TRANSPORT;


	/*
		A host address for a specific protocol.
	*/
	class HostAddress : public UsingPatternEnums
	{
	public:
		class Base;


	public:
		Transport::TRANSPORT transport = Transport::NO_TRANSPORT;
		uint16_t             number = 0;
		std::string          name;
		//std::string          path;


	public:
		// General Constructors
		HostAddress() {}
		HostAddress(Transport::TRANSPORT t, std::string_view n, uint16_t p = 0) :
			transport(t), number(p), name(n) {}

		// Transport Constructors
		static HostAddress TCP      (const std::string_view host, uint16_t port)    {return HostAddress(Transport::TCP, host, port);}
		static HostAddress TCP_Local(uint16_t port)                                 {return TCP("localhost", port);}
		static HostAddress InProc   (const std::string_view name)                   {return HostAddress(Transport::INPROC, name, 0);}
		static HostAddress IPC      (const std::string_view name)                   {return HostAddress(Transport::IPC, name, 0);}


		operator bool() const
		{
			return (transport != Transport::NO_TRANSPORT);
		}

		bool operator==(const HostAddress &o) const
		{
			return transport == o.transport &&
				((transport == Transport::TCP) ?
					(number == o.number) :
					(name == o.name));
		}


		// Get a string (URI) representation of the host port.
		operator std::string() const
		{
			std::string uri;
			switch (transport)
			{
			case Transport::INPROC:
				uri = "inproc://";
				uri.append(name);
				return uri;

			case Transport::IPC:
#if _MSC_VER
				uri = "ipc://";
#else
				uri = "ipc:///run/";
#endif
				uri.append(name);
				return uri;

			case Transport::TCP:
				uri = "tcp://";
				if (name.length()) uri.append(name);
				else               uri.append("localhost");
				uri.push_back(':');
				{
					uint16_t port_ = number;
					char portstr[5] = {'0'};
					unsigned portchars = 0u;
					while (port_ != 0)
					{
						portstr[portchars++] = '0' + (port_ % 10);
						port_ /= 10;
					}
					for (unsigned i = std::max(portchars, 1u); i-- > 0;)
						uri.push_back(portstr[i]);
				}
				return uri;

			default:
				return "unknown";
			}
		}
	};

	/*
		A range of host addresses for various protocols.
			Defines conventions for this range per transport (eg, consecutive TCP ports).
	*/
	class HostAddress::Base
	{
	public:
		HostAddress base;


	public:
		// Nil and address constructors
		Base()                             {}
		Base(const HostAddress &_value)    : base(_value) {}

		// Transport Constructors
		static Base TCP      (const std::string_view host, uint16_t port)    {return Base(HostAddress::TCP(host, port));}
		static Base TCP_Local(uint16_t port)                                 {return Base(HostAddress::TCP_Local(port));}
		static Base InProc   (const std::string_view name)                   {return Base(HostAddress::InProc(name));}
		static Base IPC      (const std::string_view name)                   {return Base(HostAddress::IPC(name));}
		
		// Nil check / comparison
		operator bool() const                   {return base;}
		bool operator==(const Base &o) const    {return base==o.base;}


		// Get address for the given communication pattern.
		HostAddress derived(Pattern::PATTERN pattern) const
		{
			switch (base.transport)
			{
			default:
				return HostAddress();

			case Transport::TCP:
				return HostAddress(base.transport, base.name, PortOffset(base.number, pattern));

			case Transport::INPROC:
			case Transport::IPC:
				break;
			}

			const char *suffix = Extension(pattern);
			return suffix ? HostAddress(base.transport, base.name + suffix, base.number) : HostAddress();
		}

		HostAddress operator[](PATTERN pattern) const
		{
			return derived(pattern);
		}


		/*
			Extension convention for inproc and IPC hosts.
		*/
		static inline const char *Extension(PATTERN p)
		{
			switch (p)
			{
			case Pattern::REQ_REP:   return ".req";
			case Pattern::PUB_SUB:   return ".sub";
			case Pattern::PUSH_PULL: return ".push";
			default: return nullptr;
			}
		}

		/*
			Consecutive port numbering convention for UDP and TCP hosts.
		*/
		static inline uint16_t PortOffset(uint16_t base_port, PATTERN p)
		{
			return base_port+uint16_t(p);
		}
	};



	/*
		Telling typically uses a pro
	*/
	inline std::string_view         DefaultServerID()         {return "telling.v0";}
	inline const HostAddress::Base& DefaultInProc()
	{
		static auto a = HostAddress::Base::InProc(DefaultServerID());
		return a;
	}
}