#pragma once


#include "msg_headers.h"
#include "msg_method.h"
#include "msg_uri.h"
#include "msg_protocol.h"
#include "msg_status.h"


namespace telling
{
	/*
		Parse a Telling message into its component parts.
	*/
	class MsgView
	{
	public:
		class Request;
		class Reply;
		class Report;

		class Exception;

		enum class TYPE : int16_t
		{
			UNKNOWN  = -1,
			REPLY   = 0, // (don't change these integer values)
			REPORT  = 1, // (they correspond to protocol's position in the start-line)
			REQUEST = 2, // (they're selected for a parsing trick)

			MASK_TYPE = 0x0F,
			FLAG_HEADER_ONLY = 0x10,
		};


	public:
		MsgView() noexcept                                        {}
		MsgView(nng::msg_view _msg, TYPE type = TYPE::UNKNOWN)    : msg(_msg), _type(type) {if (msg) {_parse_msg();}}

		~MsgView() noexcept {}


		/*
			Test validity of message view (ie, parsing success)
		*/
		explicit operator bool() const noexcept    {return _type != TYPE::UNKNOWN;}

		bool is_request() const noexcept    {return _type == TYPE::REQUEST;}
		bool is_reply  () const noexcept    {return _type == TYPE::REPLY;}
		bool is_report () const noexcept    {return _type == TYPE::REPORT;}


		/*
			Access the parts of the message:
				startLine depends on message type and may include paths, methods, or statuses.
				headers are an unordered non-unique list of key-value properties based on HTTP headers.
				data is the content of the message and can be anything (headers may describe it).
		*/

		Method            method        () const noexcept    {return Method     ::Parse(methodString());}
		UriView           uri           () const noexcept    {return UriView(_string(_uri));}
		MsgProtocol       protocol      () const noexcept    {return MsgProtocol::Parse(protocolString());}
		Status            status        () const noexcept    {return Status     ::Parse(statusString());}
		std::string_view  reason        () const noexcept    {return _string(_reason);}

		// Raw elements of start-line.
		std::string_view  startLine     () const noexcept    {return _string(0, _startLine_length);}
		std::string_view  methodString  () const noexcept    {return _string(_method);}
		std::string_view  protocolString() const noexcept    {return _string(_protocol);}
		std::string_view  statusString  () const noexcept    {return _string(_status);}

		/*
			Access the message headers, which can be iterated over.
		*/
		MsgHeaders        headers()    const noexcept    {return MsgHeaders(_string(_headers));}

		/*
			Access the message body.
		*/
		nng::view         body()       const noexcept    {return _view  (_body_offset, msg.body().size() - _body_offset);}
		std::string_view  bodyString() const noexcept    {return _string(_body_offset, msg.body().size() - _body_offset);}
		size_t            bodySize()   const noexcept    {return msg.body().size();}
		template<typename T>
		const T*          bodyData()   const noexcept    {return msg.body().data<const T>();}


	public:
		nng::msg_view    msg;


	protected:
		struct HeadRange {uint16_t start, length;};

		TYPE             _type = TYPE::UNKNOWN;

		// Basic structure
		uint16_t         _startLine_length = 0;
		uint16_t         _body_offset = 0;
		HeadRange        _headers = {};

		// Start-line properties...
		HeadRange        _method = {}, _uri = {}, _protocol = {}, _status = {}, _reason = {};


	private:
		void _parse_msg();

		const char      *_raw   ()                            const noexcept    {return msg.body().data<char>();}
		nng::view        _view  (size_t start, size_t length) const noexcept    {return nng::       view(_raw()+start, length);}
		nng::view        _view  (HeadRange b)                 const noexcept    {return nng::       view(_raw()+b.start, b.length);}
		std::string_view _string(size_t start, size_t length) const noexcept    {return std::string_view(_raw()+start, length);}
		std::string_view _string(HeadRange b)                 const noexcept    {return std::string_view(_raw()+b.start, b.length);}
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


	inline MsgView View       (nng::msg_view msg)    {return MsgView          (msg);}
	inline MsgView ViewRequest(nng::msg_view msg)    {return MsgView::Request (msg);}
	inline MsgView ViewReply  (nng::msg_view msg)    {return MsgView::Reply   (msg);}
	inline MsgView ViewReport (nng::msg_view msg)    {return MsgView::Report(msg);}
}
