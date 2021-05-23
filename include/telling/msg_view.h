#pragma once


#include "msg_headers.h"
#include "msg_method.h"
#include "msg_status.h"
#include "msg_protocol.h"


namespace telling
{
	/*
		Parse a Telling message into its component parts.
	*/
	class MsgView
	{
	public:
		// Specializations
		class Request;
		class ReplyBase;
		class Reply;
		class Bulletin;
		using Command = Request;

		class ContainsURI;

		class Exception;

	public:
		MsgView() noexcept             {}
		MsgView(nng::msg_view _msg)    : msg(_msg) {if (msg) _parse_msg();}

		~MsgView() noexcept {}

		/*
			Access the parts of the message:
				startLine depends on message type and may include paths, methods, or statuses.
				headers are an unordered non-unique list of key-value properties based on HTTP headers.
				data is the content of the message and can be anything (headers may describe it).
		*/
		std::string_view  startLine()  const noexcept    {return _string(0, startLine_length);}
		const MsgHeaders &headers()    const noexcept    {return msgHeaders;}
		nng::view         data()       const noexcept    {return _view  (body_offset, msg.body().size() - body_offset);}
		std::string_view  dataString() const noexcept    {return _string(body_offset, msg.body().size() - body_offset);}


		// Parse the essential structure of the message.
		void _parse_msg();


	public:
		nng::msg_view    msg;
		size_t           startLine_length;
		MsgHeaders       msgHeaders;
		size_t           body_offset;


	private:
		nng::view        _view  (size_t start, size_t length) const noexcept    {return nng::       view(static_cast<char*>(msg.body().data())+start, length);}
		std::string_view _string(size_t start, size_t length) const noexcept    {return std::string_view(static_cast<char*>(msg.body().data())+start, length);}
	};


	/*
		Mixin class for messages that contain a URI.
	*/
	class MsgView::ContainsURI
	{
	public:
		std::string_view uri;

	public:
		bool             uriHasPrefix(std::string_view prefix)      const    {return uri.rfind(prefix, 0) == 0;}
		std::string_view uriSubstr   (size_t offset, size_t length) const    {return uri.substr(offset, length);}
	};


	/*
		View a Request.
	*/
	class MsgView::Request : public MsgView, public MsgView::ContainsURI
	{
	public:
		Request() noexcept             {}
		Request(nng::msg_view _msg)    : MsgView(_msg) {if (msg) _parse_request();}

		~Request() noexcept {}


		void _parse_request();


	public:
		Method           method;
		std::string_view methodString;
		MsgProtocol      protocol;
		std::string_view protocolString;
	};


	/*
		Base class for Reply and Bulletin.
	*/
	class MsgView::ReplyBase : public MsgView
	{
	public:
		ReplyBase() noexcept             {}
		ReplyBase(nng::msg_view _msg)    : MsgView(_msg) {}

		~ReplyBase() noexcept {}


		Status status() const noexcept    {return Status::Parse(statusString);}


	public:
		MsgProtocol      protocol;
		std::string_view protocolString;
		std::string_view statusString;
		std::string_view reason;
	};


	/*
		View a Reply.
	*/
	class MsgView::Reply : public MsgView::ReplyBase
	{
	public:
		Reply() noexcept             {}
		Reply(nng::msg_view _msg)    : ReplyBase(_msg) {if (msg) _parse_reply();}

		~Reply() noexcept {}


		void _parse_reply();
	};


	/*
		View a Bulletin.
	*/
	class MsgView::Bulletin : public MsgView::ReplyBase, public MsgView::ContainsURI
	{
	public:
		Bulletin() noexcept             {}
		Bulletin(nng::msg_view _msg)    : ReplyBase(_msg) {if (msg) _parse_bulletin();}

		~Bulletin() noexcept {}


		void _parse_bulletin();
	};
}
