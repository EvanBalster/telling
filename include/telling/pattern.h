#pragma once

#include <string_view>


namespace telling
{
	class Pattern
	{
	public:
		enum PATTERN : int8_t
		{
			NO_PATTERN = -1,
			REQ_REP    = 0,
			PUB_SUB    = 1,
			PUSH_PULL  = 2, PIPELINE = 2,
			PAIR       = 3,
			PATTERN_COUNT
		};

		static std::string_view Name(PATTERN pattern)
		{
			switch (pattern)
			{
			case REQ_REP   : return "REQ_REP";
			case PUB_SUB   : return "PUB_SUB";
			case PUSH_PULL : return "PUSHPULL";
			case PAIR      : return "PEER";
			default:         return "none";
			}
		}
	};

	using PATTERN = Pattern::PATTERN;


	class Role
	{
	public:
		enum ROLE : int8_t
		{
			NO_ROLE = -1,
			CLIENT  = 0,
			SERVICE = 1,
			BROKER  = 2,
			PEER    = 3,
		};

		static std::string_view Name(ROLE role)
		{
			switch (role)
			{
			case CLIENT  : return "CLIENT";
			case SERVICE : return "SERVICE";
			case BROKER  : return "BROKER";
			case PEER    : return "PEER";
			default:       return "none";
			}
		}

		static std::string_view Name4c(ROLE role)
		{
			switch (role)
			{
			case CLIENT  : return "CLNT";
			case SERVICE : return "SRVC";
			case BROKER  : return "BROK";
			case PEER    : return "PEER";
			default:       return "none";
			}
		}
	};

	using ROLE = Role::ROLE;


	class Protocol
	{
	public:
		enum PROTOCOL : int8_t
		{
			NO_PROTOCOL = -1,
			PAIR = 1,
			REQ  = 2,
			REP  = 3,
			SUB  = 4,
			PUB  = 5,
			PUSH = 6,
			PULL = 7,
		};

		static std::string_view Name(PROTOCOL protocol)
		{
			switch (protocol)
			{
			case PAIR: return "PAIR";
			case REQ : return "REQ";
			case REP : return "REP";
			case SUB : return "SUB";
			case PUB : return "PUB";
			case PUSH: return "PUSH";
			case PULL: return "PULL";
			default:   return "none";
			}
		}

		// Get the client-side protocol for a pattern.
		static PROTOCOL ClientSide(PATTERN pattern)
		{
			switch (pattern)
			{
			default:                 return NO_PROTOCOL;
			case Pattern::REQ_REP:   return REQ;
			case Pattern::PUB_SUB:   return SUB;
			case Pattern::PUSH_PULL: return PUSH;
			}
		}

		// Get the server-side protocol for a pattern.
		static PROTOCOL ServerSide(PATTERN pattern)
		{
			switch (pattern)
			{
			default:                 return NO_PROTOCOL;
			case Pattern::REQ_REP:   return REP;
			case Pattern::PUB_SUB:   return PUB;
			case Pattern::PUSH_PULL: return PULL;
			}
		}

		static PROTOCOL Choose(ROLE role, PATTERN pattern)
		{
			switch (role)
			{
			case Role::CLIENT:    return ClientSide(pattern);
			case Role::SERVICE:   return ServerSide(pattern);
			case Role::BROKER:    return ServerSide(pattern);
			case Role::PEER:
				switch (pattern)
				{
				case Pattern::PAIR: return PAIR;
				default:            return NO_PROTOCOL;
				}
			default:
				return NO_PROTOCOL;
			}
		}

		// Stateful protocols support contexts.
		static bool IsStateful(PROTOCOL protocol)
		{
			switch (protocol)
			{
			default:   return false;
			case PAIR: return false;
			case REQ : return true;
			case REP : return true;
			case SUB : return true;
			case PUB : return false;
			case PUSH: return false;
			case PULL: return true;
			}
		}
	};

	using PROTOCOL = Protocol::PROTOCOL;



	/*
		Inheritable class for importing pattern enumerations.
			No data members.
	*/
	class UsingPatternEnums : public Pattern, public Role, public Protocol
	{
	};
}