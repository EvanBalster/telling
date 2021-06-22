#pragma once

#include <ostream>
#include <string>
#include "util/HttpStatusCodes_C++11.h"


namespace telling
{
	using StatusCode = HttpStatus::Code;


	class Status
	{
	public:
		StatusCode code;

	public:
		Status()                : code(StatusCode(0)) {}
		Status(StatusCode c)    : code(c) {}

		// Check status validity -- DOES NOT DISTINGUISH SUCCESS FROM ERROR
		explicit operator bool() const noexcept    {return toInt() > 0;}

		// Convert implicitly to code
		operator StatusCode() const noexcept    {return code;}

		// Convert to integer
		int  toInt          () const noexcept    {return HttpStatus::toInt(code);}

		// String conversion
		static Status    Parse(std::string_view) noexcept;
		std::string      toString()        const noexcept;

		// Categorize the status according to HTTP conventions.
		bool isInformational() const noexcept    {return HttpStatus::isInformational(code);}
		bool isSuccessful   () const noexcept    {return HttpStatus::isSuccessful(code);}
		bool isRedirection  () const noexcept    {return HttpStatus::isRedirection(code);}
		bool isClientError  () const noexcept    {return HttpStatus::isClientError(code);}
		bool isServerError  () const noexcept    {return HttpStatus::isServerError(code);}
		bool isError        () const noexcept    {return HttpStatus::isError(code);}

		// Shorthand methods
		bool isInfo         () const noexcept    {return isInformational();}
		bool isSuccess      () const noexcept    {return isSuccessful();}
		bool isRedirect     () const noexcept    {return isRedirection();}
		
		// Get reason-phrase string
		std::string_view reasonPhrase() const noexcept
		{
			auto rp = HttpStatus::reasonPhrase(code);
			if (rp.length() == 0) rp = "(Undefined Status)";
			return rp;
		}
	};


	inline std::string Status::toString() const noexcept
	{
		int n = toInt();
		if (n <= 0 || n > 999) return std::string("N/A");

		std::string res; res.resize(3);
		res[0] = '0' + ((n/100)%10);
		res[1] = '0' + ((n/ 10)%10);
		res[2] = '0' + ((n    )%10);
		return res;
	}

	inline Status Status::Parse(std::string_view s) noexcept
	{
		if (s.size() != 3
			|| s[0] < '0' || s[0] > '9'
			|| s[1] < '0' || s[1] > '9'
			|| s[2] < '0' || s[2] > '9')
		{
			return Status();
		}
		return StatusCode
			(       int(s[2]-'0')
			+  10 * int(s[1]-'0')
			+ 100 * int(s[0]-'0'));
	}
}

inline std::ostream &operator<<(std::ostream &out, const telling::Status s)
{
	return out << s.toString();
}