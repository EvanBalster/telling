#pragma once


#include <string_view>


namespace telling
{
	enum class MethodCode
	{
		Unknown = -1,
		None    = 0,
		GET,
		HEAD,
		POST,
		PUT,
		DELETE,
		PATCH,
		OPTIONS,
		CONNECT,
		TRACE,
		EndOfValidMethods
	};


	/*
		Represents an HTTP method.
	*/
	class Method
	{
	public:
		MethodCode code;

	public:
		Method()             noexcept                  : code(MethodCode::None) {}
		Method(MethodCode c) noexcept                  : code(c)                {}

		// String conversion
		static Method    Parse(std::string_view) noexcept;
		std::string_view toString()        const noexcept;

		// Categorize the method according to HTTP conventions.
		bool isSafe()        const;
		bool isNullipotent() const    {return isSafe();}
		bool isIdempotent()  const;
		bool isCacheable()   const;

		// Is a body expected with this method?
		bool allowRequestBody()  const;
		bool allowResponseBody() const;

		// Does it make sense to use this method without a response?
		bool allowNoResponse() const;

		// Check method validity
		explicit operator bool() const noexcept    {return code > MethodCode::None && code < MethodCode::EndOfValidMethods;}
	};


	/*
		Represents a set of HTTP methods.
	*/
	class Methods
	{
	public:
		using mask_t = uint32_t;
		mask_t mask = 0;

	public:
		Methods()             noexcept    {}
		Methods(MethodCode m) noexcept    {insert(m);}
		Methods(Method     m) noexcept    {insert(m);}

		static Methods All() noexcept    {Methods m; m.mask = ~mask_t(1u); return m;}

		void     clear     ()               noexcept    {mask = 0;}
		void     insert    (Method m)       noexcept    {if (m.code > MethodCode::None) mask |=  (mask_t(1u) << mask_t(m.code));}
		void     erase     (Method m)       noexcept    {mask &= ~(mask_t(1u) << mask_t(m.code));}
		bool     contains  (Method m) const noexcept    {return (mask >> mask_t(m.code)) & mask_t(1u);}

		Methods  operator+ (Method m) noexcept    {Methods r(*this); r.insert(m); return r;}
		Methods  operator- (Method m) noexcept    {Methods r(*this); r.erase (m); return r;}

		Methods& operator+=(Method m) noexcept    {insert(m); return *this;}
		Methods& operator-=(Method m) noexcept    {erase (m); return *this;}
	};


	inline Methods operator+(Method a, Method b) noexcept
	{
		Methods m;
		m.insert(a);
		m.insert(b);
		return m;
	}


	/*
		IMPLEMENTATION FOLLOWS
	*/

	inline bool Method::isSafe()        const
	{
		switch (code)
		{
		case MethodCode::GET:
		case MethodCode::HEAD:
		case MethodCode::OPTIONS:
		case MethodCode::TRACE:
			return true;
		default:
			return false;
		}
	}
	inline bool Method::isIdempotent()  const
	{
		switch (code)
		{
		case MethodCode::GET:
		case MethodCode::HEAD:
		case MethodCode::PUT:
		case MethodCode::DELETE:
		case MethodCode::OPTIONS:
		case MethodCode::TRACE:
			return true;
		default:
			return false;
		}
	}
	inline bool Method::isCacheable()   const
	{
		switch (code)
		{
		case MethodCode::GET:
		case MethodCode::HEAD:
		case MethodCode::POST:
			return true;
		default:
			return false;
		}
	}
	inline bool Method::allowRequestBody() const
	{
		switch (code)
		{
		case MethodCode::HEAD:
		case MethodCode::DELETE:
		case MethodCode::TRACE:
			return false;
		default:
			return true;
		}
	}
	inline bool Method::allowResponseBody() const
	{
		switch (code)
		{
		case MethodCode::HEAD:
			return false;
		default:
			return true;
		}
	}

	inline bool Method::allowNoResponse() const
	{
		switch (code)
		{
		case MethodCode::GET:
		case MethodCode::HEAD:
		case MethodCode::OPTIONS:
		case MethodCode::CONNECT:
		case MethodCode::TRACE:
			return false;
		default:
			return true;
		}
	}

	inline std::string_view Method::toString() const noexcept
	{
		#define TELLING_METHOD_STRING_CASE(NAME) case MethodCode::NAME: return #NAME

		switch (code)
		{
			TELLING_METHOD_STRING_CASE(GET);
			TELLING_METHOD_STRING_CASE(HEAD);
			TELLING_METHOD_STRING_CASE(POST);
			TELLING_METHOD_STRING_CASE(PUT);
			TELLING_METHOD_STRING_CASE(DELETE);
			TELLING_METHOD_STRING_CASE(PATCH);
			TELLING_METHOD_STRING_CASE(OPTIONS);
			TELLING_METHOD_STRING_CASE(TRACE);
			TELLING_METHOD_STRING_CASE(CONNECT);

			case MethodCode::None:    return "NoMethod";
			default:
			case MethodCode::Unknown: return "UnknownMethod";
		}

		#undef TELLING_METHOD_STRING_CASE
	}

	namespace detail
	{
		constexpr unsigned CC2(char a, char b)    {return (unsigned(a)<<8u) | unsigned(b);}
	}

	inline Method Method::Parse(std::string_view v) noexcept
	{
		using namespace detail;
		#define TELLING_METHOD_PARSE_CASE(NAME) \
			case detail::CC2(#NAME[0], #NAME[1]): return (v == #NAME) ? MethodCode::NAME : MethodCode::Unknown

		switch (v.length())
		{
		case 0:
			return MethodCode::None;
		case 1:
			// TODO consider nonstandard single-byte method codes for internal use
			return MethodCode::Unknown;
		default:
			switch (detail::CC2(v[0], v[1]))
			{
				TELLING_METHOD_PARSE_CASE(GET);
				TELLING_METHOD_PARSE_CASE(HEAD);
				TELLING_METHOD_PARSE_CASE(POST);
				TELLING_METHOD_PARSE_CASE(PUT);
				TELLING_METHOD_PARSE_CASE(DELETE);
				TELLING_METHOD_PARSE_CASE(PATCH);
				TELLING_METHOD_PARSE_CASE(OPTIONS);
				TELLING_METHOD_PARSE_CASE(TRACE);
				TELLING_METHOD_PARSE_CASE(CONNECT);
				default: return MethodCode::Unknown;
			}
		}

		#undef TELLING_METHOD_PARSE_CASE
	}
}

inline std::ostream &operator<<(std::ostream &out, const telling::Method m)
{
	return out << m.toString();
}
