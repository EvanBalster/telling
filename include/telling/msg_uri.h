#pragma once


#include <string_view>


namespace telling
{
	template<typename StdStringClass> class Uri_;
	using UriView = Uri_<std::string_view>;
	using Uri     = Uri_<std::string>;


	/*
		An adaptation of std::string and std::string_view for URIs.
	*/
	template<typename T_String>
	class Uri_ : public T_String
	{
	public:
		// Constructors same as base class
		//using T_String::T_String;
		Uri_()                             : T_String() {}
		Uri_(const char *c_str)            : T_String(c_str) {}
		Uri_(const char *s, size_t len)    : T_String(s, len) {}
		Uri_(const std::string_view &s)    : T_String(s) {}
		Uri_(const std::string      &s)    : T_String(s) {}
		Uri_(const Uri              &s)    : T_String(s) {}
		Uri_(const UriView          &s)    : T_String(s) {}


	public:
		/*
			UriView is truthy if it points to a non-zero address, even if its length is zero.
		*/
		explicit operator bool() const    {return T_String::data() != nullptr;}

		bool    hasPrefix(std::string_view prefix)                    const    {return T_String::rfind(prefix, 0) == 0;}
		UriView substr   (size_t pos, size_t len = std::string::npos) const    {return T_String::data() ? std::string_view(*this).substr(pos, T_String::length()) : Uri_();}

		/*
			If the URI matches this prefix, returns the remainder of the URI (which is truthy).
				Otherwise, returns an empty, falsy URI.
		*/
		UriView subpath  (std::string_view prefix)                    const    {return hasPrefix(prefix) ? std::string_view(*this).substr(prefix.length()) : Uri_();}
	};
}