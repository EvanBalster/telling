#pragma once


#include <string_view>
#include <nngpp/msg.h>

#include "msg_util.h"


namespace telling
{
	/*
		Views an HTTP-like header (name & value separated by ':') in a string.
			If the string contains multiple lines, only the first will be parsed.
	*/
	class MsgHeaderView
	{
	public:
		MsgHeaderView()                      noexcept    {}
		MsgHeaderView(std::string_view line) noexcept    {parse_header(line);}

		explicit operator bool() const    {return name.length() || value.length();}


		void parse_header(std::string_view line) noexcept;

	public:
		std::string_view name;
		std::string_view value;
	};

	/*
		A class for reading HTTP-formatted message headers.
	*/
	class MsgHeaders
	{
	public:
		class iterator
		{
		public:
			iterator(const char *p, const char *e)      : end(e) {setp(p);}
			bool operator==(const iterator &o) const    {return getp()==o.getp();}
			bool operator!=(const iterator &o) const    {return getp()!=o.getp();}
			iterator& operator++()                      {auto *p = getp(); detail::ConsumeLine(p, end); setp(p); return *this;}

			const MsgHeaderView& operator* () const    {return header;}
			const MsgHeaderView* operator->() const    {return &this->operator*();}

		private:
			mutable MsgHeaderView header;
			const char *end;

			const char *getp() const           {return header.name.data();}
			void        setp(const char *p)
			{
				header = MsgHeaderView();
				if (p < end) {const char *tmp = p; header = MsgHeaderView(detail::ConsumeLine(tmp, end));}
				if (!header.name.length()) header.name = std::string_view(p, 0);
			}
		};

	public:
		iterator begin() const    {return iterator(string.data(), string.data()+string.length());}
		iterator end  () const    {auto e = string.data()+string.length(); return iterator(e, e);}

	public:
		std::string_view string;
	};


	/*
		Parse a message header from the given string view.
	*/
	inline void MsgHeaderView::parse_header(std::string_view line) noexcept
	{
		const char *i = line.data(), *e = i+line.length();

		// Parse header (or, failing to find header, return line as value).
		while (true)
		{
			if (i == e || *i == '\r' || *i == '\n')
			{
				// Name not found; parse entire line as value.
				name = std::string_view(line.data(), 0);
				i = line.data();
				break;
			}
			if (*i == ':')
			{
				// Name found; parse remainder as value.
				name = std::string_view(line.data(), i-line.data());
				++i;
				break;
			}
			++i;
		}

		// Consume spaces/tabs before value
		while (i != e && (*i == ' ' || *i == '\t')) ++i;

		// Consume value
		const char *v_first = i, *v_last = i;
		while (i != e)
		{
			// Trailing spaces/tabs aren't part of value
			if (*i != ' ' && *i != '\t') v_last = i+1;
			++i;
		}

		// Did we find a value?
		value = std::string_view(v_first, v_last-v_first);
	}
}