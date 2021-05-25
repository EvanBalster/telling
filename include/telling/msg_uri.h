#pragma once


#include <string_view>


namespace telling
{
	/*
		An adaptation of std::string_view for URIs.
	*/
	class UriView : public std::string_view
	{
	public:
		UriView()                              : std::string_view() {}
		UriView(const std::string_view &other) : std::string_view(other) {}
		UriView(const std::string      &other) : std::string_view(other) {}
		UriView(const char *s, size_t count)   : std::string_view(s, count) {}
		UriView(const char *cstr)              : std::string_view(cstr) {}

	public:
		/*
			UriView is truthy if it points to a non-zero address, even if its length is zero.
		*/
		explicit operator bool() const    {return data() != nullptr;}

		bool    hasPrefix(std::string_view prefix)                    const    {return rfind(prefix, 0) == 0;}
		UriView substr   (size_t pos, size_t len = std::string::npos) const    {return data() ? std::string_view::substr(pos, length()) : UriView();}

		/*
			If the URI matches this prefix, returns the remainder of the URI (which is truthy).
				Otherwise, returns an empty, falsy URI.
		*/
		UriView subpath  (std::string_view prefix)                    const    {return hasPrefix(prefix) ? std::string_view::substr(prefix.length()) : UriView();}
	};
}