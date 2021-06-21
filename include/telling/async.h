#pragma once


#include <memory>
#include <string_view>

#include <nngpp/aio.h>
#include <nngpp/core.h>


namespace telling
{
	/*
		Recyclable unique ID used for request/reply patterns.
	*/
	using QueryID = decltype(nng_ctx::id);


	class Socket;


	/*
		Data structure for an asynchronous error message.
	*/
	struct AsyncError
	{
		nng::error       nng_status;
		std::string_view error_msg;

		std::string_view what() const noexcept
		{
			return
				error_msg.length() ? error_msg :
				bool(nng_status)   ? nng::to_string(nng_status) :
				"success";
		}

		operator nng::error      () const noexcept    {return nng_status;}
		operator std::string_view() const noexcept    {return what();}

		AsyncError()                : nng_status(nng::error::success) {}
		AsyncError(nng::error e)    : nng_status(e)                   {}
	};


	/*
		Asynchronous event handlers in this library derive from this base.
			The class is templated with a "tag" type that is passed to all methods.
			Tag does not need to contain any information.
	*/
	template<typename Tag>
	class AsyncHandler
	{
	public:
		virtual ~AsyncHandler()               {}

		/*
			Common asynchronous events:
				start -- an I/O begins using the handler.
				stop  -- an I/O finishes using the handler.

			These messages bracket all other async calls from a given source.
		*/
		virtual void async_start(Tag)                       {}
		virtual void async_stop (Tag, AsyncError status)    {}

		/*
			Failed to send or receive some message.
			Common errors:
				nng::error::canceled -- this action was canceled or the communicator shut down
				nng::error::timedout -- ran out of time to send/receive a message
		*/
		virtual void async_error(Tag, AsyncError error)     {}
	};


	/*
		Callback interface for receiving messages.
			Used for PULL and SUBscribe protocols.
			Tag is provided as a parameter to all methods of this interface.
			Tag can be used to distinguish between protocols or callers.
	*/
	template<typename Tag>
	class AsyncRecv : public virtual AsyncHandler<Tag>
	{
	public:
		virtual ~AsyncRecv() {}

		/*
			Called when a message is received.
				Return <message> to respond (if the protocol supports this).
				Return CONTINUE to accept the message and continue receiving.
				Return DECLINE to discard the message and continue receiving.
				Return TERMINATE to stop receiving messages.
				Return AUTO to let the system decide.
		*/
		virtual void async_recv(Tag, nng::msg &&msg) = 0;
	};

	/*
		Callback interface for sending messages.
			Used for PUSH and PUBlish protocols.
	*/
	template<class Tag>
	class AsyncSend : public virtual AsyncHandler<Tag>
	{
	public:
		using QueryID   = telling::QueryID;

	public:
		virtual ~AsyncSend() {}

		/*
			A message has been prepared for sending.  (Called from producer)
				Return std::move(msg) to transmit it now.
				Return CONTINUE if the message was queued for later sending.
				Return DECLINE if the message is discarded.
			
			It is appropriate to throw exceptions from this method,
				when messages can't be accepted for some reason.
		*/
		virtual void async_prep(Tag, nng::msg &msg) = 0;

		/*
			A message has been sent successfully.  (Called from AIO system)
				Return <a message> to transmit another message now, if supported.
				Return CONTINUE to finish up.
		*/
		virtual void async_sent(Tag)           = 0;

		/*
			NOTE:
				AsyncSend often has concurrency responsibilities:
				"send" should enqueue the message if another is sending or return it if not.
				"sent" notifies that one message has finished sending.
		*/
	};


	/*
		Callback interface for sending queries and getting responses.
			Used for REQuest protocol, maybe Surveyor in the future.

		Tag may be a "query ID" used for tracking pending requests.
	*/
	template<class Tag>
	class AsyncQuery : public AsyncSend<Tag>, public AsyncRecv<Tag>
	{
	public:
		virtual ~AsyncQuery() {}

		/*
			Asynchronous events (see AsyncSend & AsyncRecv)
				AsyncSend   ::async_prep  -- a new request (may be modified or deleted)
				AsyncSend   ::async_sent  -- a request has been sent.
				AsyncRecv   ::async_recv  -- a response has been received.
				AsyncHandler::async_error -- the query failed somehow.
		*/
		virtual void async_prep(Tag, nng::msg &)    {}    // Optional
	};


	/*
		Callback interface for responding to messages.
			Used for REPly protocol, maybe Responder in the future.

		Tag may be a "query ID" used for tracking pending requests.
	*/
	template<class Tag>
	class AsyncRespond : public AsyncRecv<Tag>, public AsyncSend<Tag>
	{
	public:
		virtual ~AsyncRespond() {}

		/*
			Asynchronous events (see AsyncSend & AsyncRecv)
				AsyncRecv   ::async_recv  -- a request has been received (may return a reply)
				AsyncSend   ::async_prep  -- a response has been prepared (may be modified or deleted)
				AsyncSend   ::async_sent  -- a response has been sent.
				AsyncHandler::async_error -- failed to handle a request or response.

			The Tag passed to async_recv will typically provide a method for replying,
				either immediately from async_recv and/or later on.
		*/
		virtual void async_prep(Tag, nng::msg &)    {}    // Optional
	};



	/*
		A feature for tags that support reacting to an event with a message.

		Used...
			- in AsyncSend::async_sent, to send another message immediately
			- in AsyncRespond::async_recv, to send a response immediately
	*/
	struct SendPrompt
	{
		nng::msg *_msg;

		void setDest(nng::msg &msg)           noexcept    {_msg = &msg;}

		explicit operator bool()        const noexcept    {return _msg;}
		void operator()(nng::msg &&msg) const noexcept    {if (_msg) *_msg = std::move(msg);}
	};


	/*
		Optional conventions for async I/O tags.
			AsyncSendLoop depends on SendPrompt's behavior.
	*/
	template<class Comm> struct TagRecv    {Comm *comm;};
	template<class Comm> struct TagSend    {Comm *comm;             SendPrompt send;};
	template<class Comm> struct TagQuery   {Comm *comm; QueryID id;};
	template<class Comm> struct TagRespond {Comm *comm; QueryID id; SendPrompt send;};

	// Void specializations of the above templates.
	template<> struct TagRecv   <void> {};
	template<> struct TagSend   <void> {            SendPrompt send;};
	template<> struct TagQuery  <void> {QueryID id;};
	template<> struct TagRespond<void> {QueryID id; SendPrompt send;};
}
