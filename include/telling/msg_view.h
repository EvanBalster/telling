#pragma once


#include "msg_headers.h"
#include "msg_method.h"
#include "msg_uri.h"
#include "msg_protocol.h"
#include "msg_status.h"
#include "msg_layout.h"
#include "msg_stream.h"


namespace telling
{
	/*
		MsgView parses a message according to Telling's HTTP-like format.
	*/
	class MsgView : protected MsgLayout
	{
	public:
		class Request;
		class Reply;
		class Report;

		class Exception;

		using TYPE = MsgLayout::TYPE;


	public:
		MsgView() noexcept                        {_parse_reset();}
		MsgView(nng::msg_view _msg)               : msg(_msg) {if (msg) {_parse_msg(msg.body().get());}}
		MsgView(nng::msg_view _msg, TYPE type)    : msg(_msg) {if (msg) {_parse_msg(msg.body().get(), type);}}

		~MsgView() noexcept {}


		/*
			Test validity of message view (ie, parsing success)
		*/
		explicit operator bool() const noexcept    {return _type() != TYPE::UNKNOWN;}

		bool is_request() const noexcept    {return _type() == TYPE::REQUEST;}
		bool is_reply  () const noexcept    {return _type() == TYPE::REPLY;}
		bool is_report () const noexcept    {return _type() == TYPE::REPORT;}

		TYPE msgType   () const noexcept    {return _type();}


		/*
			Access the parts of the message:
				startLine depends on message type and may include paths, methods, or statuses.
				headers are an unordered non-unique list of key-value properties based on HTTP headers.
				data is the content of the message and can be anything (headers may describe it).
		*/
		Method           method        () const noexcept    {return Method     ::Parse(  methodString());}
		UriView          uri           () const noexcept    {return UriView(                uriString());}
		MsgProtocol      protocol      () const noexcept    {return MsgProtocol::Parse(protocolString());}
		Status           status        () const noexcept    {return Status     ::Parse(  statusString());}
		std::string_view reason        () const noexcept    {return _string(_reason());}

		// Raw elements of start-line.
		std::string_view startLine     () const noexcept    {return _string(_startLine());}
		std::string_view      uriString() const noexcept    {return _string(_uri());}
		std::string_view   methodString() const noexcept    {return _string(_method());}
		std::string_view protocolString() const noexcept    {return _string(_protocol());}
		std::string_view   statusString() const noexcept    {return _string(_status());}

		/*
			Access the message headers, which can be iterated over.
		*/
		MsgHeaders        headers()    const noexcept    {return MsgHeaders(_string_rem_nl(_headers()));}

		/*
			Access the message body.
		*/
		nng::view         body()       const noexcept    {return _view  (_p_body, bodySize());}
		std::string_view  bodyString() const noexcept    {return _string(_p_body, bodySize());}
		size_t            bodySize()   const noexcept    {return msg.body().size() - _p_body;}
		template<typename T>
		const T*          bodyData()   const noexcept    {return msg.body().data<const char>() + _p_body;}

		nng::imsgstream   readBody()   const noexcept    {return nng::imsgstream(msg);}



	public:
		nng::msg_view    msg;


	private:
		HeadRange _body() const noexcept    {return {_p_body, (_p_body ? msg.body().size() : 0) - _p_body};}

		const char      *_raw   ()                            const noexcept    {return msg.body().data<char>();}
		nng::view        _view  (size_t start, size_t length) const noexcept    {return nng::       view(_raw()+start, length);}
		nng::view        _view  (HeadRange b)                 const noexcept    {return nng::       view(_raw()+b.start, b.length);}
		std::string_view _string(size_t start, size_t length) const noexcept    {return std::string_view(_raw()+start, length);}
		std::string_view _string(HeadRange b)                 const noexcept    {return std::string_view(_raw()+b.start, b.length);}
		std::string_view _string_rem_nl(HeadRange b) const noexcept
		{
			std::string_view s(_string(b));
			if (            s.back() == '\n') s.remove_suffix(1);
			if (s.size() && s.back() == '\r') s.remove_suffix(1);
			return s;
		}
	};


	class MsgView::Request : public MsgView
	{
	public:
		~Request() noexcept {}
		Request()  noexcept {}
		Request(nng::msg_view msg)     : MsgView(msg, TYPE::REQUEST) {}
	};
	
	class MsgView::Reply : public MsgView
	{
	public:
		~Reply() noexcept {}
		Reply()  noexcept {}
		Reply(nng::msg_view msg)       : MsgView(msg, TYPE::REPLY) {}
	};

	class MsgView::Report : public MsgView
	{
	public:
		~Report() noexcept {}
		Report()  noexcept {}
		Report(nng::msg_view msg)    : MsgView(msg, TYPE::REPORT) {}
	};


	inline MsgView View       (nng::msg_view msg)    {return MsgView         (msg);}
	inline MsgView ViewRequest(nng::msg_view msg)    {return MsgView::Request(msg);}
	inline MsgView ViewReply  (nng::msg_view msg)    {return MsgView::Reply  (msg);}
	inline MsgView ViewReport (nng::msg_view msg)    {return MsgView::Report (msg);}
}
