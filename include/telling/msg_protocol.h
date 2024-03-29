#pragma once


#include <string_view>


namespace telling
{
	enum class MsgProtocolCode
	{
		Unknown  = -1,
		None     = 0,
		Telling  = 1,
		Http_1_0 = 2,
		Http_1_1 = 3,
		Http     = 3,
	};


	/*
		Represents an HTTP method.
	*/
	class MsgProtocol
	{
	public:
		MsgProtocolCode code;

	public:
		constexpr MsgProtocol()                  noexcept                  : code(MsgProtocolCode::None) {}
		constexpr MsgProtocol(MsgProtocolCode c) noexcept                  : code(c)                     {}

		// String conversion
		static MsgProtocol Parse(std::string_view) noexcept;
		std::string_view   toString()        const noexcept;

		// Check method validity
		explicit operator bool() const noexcept    {return code > MsgProtocolCode::None && code <= MsgProtocolCode::Http;}

		// Properties...
		std::string_view  preferred_newline() const noexcept    {return (code <= MsgProtocolCode::Telling) ? "\n" : "\r\n";}


		bool is_http() const noexcept    {return code==MsgProtocolCode::Http_1_0 || code==MsgProtocolCode::Http_1_1;}
	};

	/*
		Constants...
	*/
	static constexpr MsgProtocol
		Http_1_0 = MsgProtocolCode::Http_1_0,
		Http_1_1 = MsgProtocolCode::Http_1_1,
		Http     = MsgProtocolCode::Http,
		Telling  = MsgProtocolCode::Telling;
	

	inline std::string_view MsgProtocol::toString() const noexcept
	{
		switch (code)
		{
			case MsgProtocolCode::Telling:  return "Tell/0";
			case MsgProtocolCode::Http_1_0: return "HTTP/1.0";
			case MsgProtocolCode::Http_1_1: return "HTTP/1.1";

			case MsgProtocolCode::None:     return "NoProtocol";
			default:
			case MsgProtocolCode::Unknown:  return "UnknownProtocol";
		}
	}

	inline MsgProtocol MsgProtocol::Parse(std::string_view v) noexcept
	{
		using namespace detail;

		if (v.length() == 0)     return MsgProtocolCode::None;
		if      (v[0] == 'T')
		{
			if (v == "Tell/0")   return MsgProtocolCode::Telling;
		}
		else if (v[0] == 'H')
		{
			if (v == "HTTP/1.0") return MsgProtocolCode::Http_1_0;
			if (v == "HTTP/1.1") return MsgProtocolCode::Http_1_1;
		}

		return MsgProtocolCode::Unknown;
	}
}

inline std::ostream &operator<<(std::ostream &out, const telling::MsgProtocol m)
{
	return out << m.toString();
}
