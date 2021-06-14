#pragma once


#include <memory>
#include <nngpp/aio.h>
#include <nngpp/core.h>
#include <nngpp/ctx.h>


namespace telling
{
	class Directive;

	/*
		Recyclable unique ID used for request/reply patterns.
	*/
	using QueryID = decltype(nng_ctx::id);

	/*
		A trivial empty base class for asynchronous event handlers.
			Provides some useful symbols.
	*/
	class AsyncEnums
	{
	public:
		enum DIRECTIVE
		{
			// Default behavior based on status.
			AUTO      = 0,

			// Proceed with communications.
			CONTINUE  = 1,

			// Discard a message but continue communications.
			DECLINE   = 2,

			// Stop communications, canceling any which are in progress.
			TERMINATE = 3,
		};

		using Directive = telling::Directive;
		using QueryID   = telling::QueryID;
	};

	/*
		Directives are returned by asynchronous event handlers in order to direct
			what should happen next:
	*/
	class Directive : public AsyncEnums
	{
	public:
		Directive()               noexcept    : _code(AUTO)                               {}
		Directive(Directive &&o)  noexcept    : _code(o._code), _msg(std::move(o._msg)) {}
		Directive(DIRECTIVE  d)   noexcept    : _code(d)                                  {}
		Directive(nng::msg &&msg) noexcept    : _code(AUTO),    _msg(std::move(msg))     {}

		// Access fields
		DIRECTIVE  directive  () noexcept    {return _code;}
		nng::msg  &msg        () noexcept    {return _msg;}
		nng::msg &&release_msg() noexcept    {return std::move(_msg);}

		// Implicit conversion for switch statements
		operator DIRECTIVE() const noexcept    {return _code;}

		// Assignment & moving
		Directive& operator=(Directive &&o) noexcept    {_code = o._code; _msg = std::move(o._msg); return *this;}
		Directive& operator=(DIRECTIVE  d)  noexcept    {_code = d;       _msg = {};                return *this;}
		Directive& operator=(nng::msg &&m)  noexcept    {_code = AUTO;    _msg = std::move(m);      return *this;}

	private:
		DIRECTIVE _code;
		nng::msg  _msg;

		// No copying, this type may only be moved.
		Directive     (const Directive&) = delete;
		void operator=(const Directive&) = delete;
	};


	class Socket;


	/*
		Callback interface for receiving messages.
			Used for PULL and SUBscribe protocols.
			Tag is provided as a parameter to all methods of this interface.
			Tag can be used to distinguish between protocols or callers.
	*/
	class AsyncRecv : public AsyncEnums
	{
	public:
		using Directive = telling::Directive;

	public:
		virtual ~AsyncRecv() {}

		/*
			Asynchronous events.
				Only one call with occur at a time per caller.
				
				asyncRecv_start -- optional; throw nng::exception to prevent
				asyncRecv_msg   -- required; receive a message
				asyncRecv_error -- recommended; error occurred receiving message
				asyncRecv_stop  -- optional
		*/
		virtual void      asyncRecv_start()                      {}
		virtual Directive asyncRecv_msg  (nng::msg &&msg)        = 0;
		virtual Directive asyncRecv_error(nng::error status)     {return AUTO;}
		virtual void      asyncRecv_stop (nng::error status)     {}


	public:
		/*
			Optional base class for AIO receiver that calls an AsyncRecv object.
		*/
		template<class T_RecvCtx>
		class Cycle_
		{
		public:
			Cycle_(T_RecvCtx &&_ctx);
			~Cycle_();

			// Start/stop receiving.  Start may throw exceptions on failure.
			void recv_start(std::weak_ptr<AsyncRecv> _delegate);
			void recv_stop ()  noexcept;

			T_RecvCtx       &recv_ctx()       noexcept    {return _recv_ctx;}
			const T_RecvCtx &recv_ctx() const noexcept    {return _recv_ctx;}

			std::weak_ptr<AsyncRecv> recv_delegate() const    {return _recv_delegate;}

		private:
			nng::aio                 _recv_aio;
			T_RecvCtx                _recv_ctx;
			std::weak_ptr<AsyncRecv> _recv_delegate;
		};

		using Cycle    = Cycle_<nng::socket_view>;
		using CtxCycle = Cycle_<nng::ctx>;
	};

	/*
		Callback interface for sending messages.
			Used for PUSH and PUBlish protocols.
		
		After a successful send, we may send another message by returning
			CONTINUE and filling in sendMsg.
	*/
	class AsyncSend : public AsyncEnums
	{
	public:
		using Directive = telling::Directive;

	public:
		virtual ~AsyncSend() {}

		/*
			asyncSend has concurrency responsibilities.
				"msg" should enqueue the message if another is sending or return it if not.
				"sent" notifies that one message has finished sending.
		*/
		virtual Directive asyncSend_msg  (nng::msg &&msg)       = 0;
		virtual Directive asyncSend_sent ()                     = 0;
		virtual Directive asyncSend_error(nng::error status)    {return AUTO;}
		virtual void      asyncSend_stop (nng::error status)    {}


	public:
		/*
			Optional base class for AIO sender that calls an AsyncSend object.
		*/
		template<class T_SendCtx>
		class Cycle_
		{
		public:
			Cycle_(T_SendCtx &&_ctx);
			~Cycle_();

			/*
				send_msg may throw an exception if the delegate refuses.
				send_stop halts sending.
			*/
			void send_init(std::weak_ptr<AsyncSend> _delegate);
			void send_msg (nng::msg &&msg);
			void send_stop()              noexcept;

			T_SendCtx       &send_ctx()       noexcept    {return _send_ctx;}
			const T_SendCtx &send_ctx() const noexcept    {return _send_ctx;}

			std::weak_ptr<AsyncSend> send_delegate() const    {return _send_delegate;}

		private:
			nng::aio                 _send_aio;
			T_SendCtx                _send_ctx;
			std::weak_ptr<AsyncSend> _send_delegate;
		};

		using Cycle    = Cycle_<nng::socket_view>;
		using CtxCycle = Cycle_<nng::ctx>;
	};


	/*
		Callback interface for sending queries and getting responses.
			Used for REQuest protocol, maybe Surveyor in the future.
	*/
	class AsyncQuery : public AsyncEnums
	{
	public:
		virtual ~AsyncQuery() {}

		/*
			Asynchronous events.
				"made" notes a newly-composed request, which may be declined.
				"sent" notifies that one message has finished sending.
				"done" delivers the response.
				"error" signals that the query has failed or terminated.

			QueryID may be reused after "done" or "error".
		*/
		virtual Directive asyncQuery_made (QueryID, const nng::msg &query)  {return CONTINUE;}
		virtual Directive asyncQuery_sent (QueryID)                         {return CONTINUE;}
		virtual Directive asyncQuery_done (QueryID, nng::msg &&response)    = 0;
		virtual Directive asyncQuery_error(QueryID, nng::error status)      = 0;
	};


	/*
		Callback interface for responding to messages.
			Used for REPly protocol, maybe Responder in the future.
	*/
	class AsyncRespond : public AsyncEnums
	{
	public:
		virtual ~AsyncRespond() {}

		/*
			Asynchronous events.
				"recv" delivers a new query, providing for immediate or later response.
				"done" signals that the response has been sent.
				"error" signals that the query has failed or terminated.

			QueryID may be reused after "done" or "error".

			If a message is not returned from "recv", behavior is caller-dependent;
				Where possible, communicators should offer a method for delayed response.
		*/
		virtual Directive asyncRespond_recv (QueryID, nng::msg &&query)  = 0;
		virtual void      asyncRespond_done (QueryID)                    = 0;
		virtual Directive asyncRespond_error(QueryID, nng::error status) = 0;
	};


	/*
		Implementation stuff follows...
	*/
	namespace detail
	{
		template<auto Member_aio, auto Member_ctx, auto Member_delegate, typename T_Self>
		void AsyncRecv_Callback_Self(void *_self)
		{
			auto *self = static_cast<T_Self*>(_self);
			nng::aio   &aio      =  self->*Member_aio;
			auto       &ctx      =  self->*Member_ctx;
			const auto  delegate = (self->*Member_delegate).lock();

			nng::error aioResult = aio.result();

			if (!delegate)
			{
				// Stop receiving if there is no delegate.
				if (aioResult == nng::error::success) aio.release_msg();
				return;
			}

			Directive directive =
				(aioResult == nng::error::success)
				? delegate->asyncRecv_msg  (aio.release_msg())
				: delegate->asyncRecv_error(aioResult);

			switch (directive)
			{
			default:
			case Directive::AUTO:
				if (aioResult != nng::error::success)
				{
					[[fallthrough]]; case Directive::TERMINATE:
					delegate->asyncRecv_stop(aioResult);
					return;
				}
				else
				{
					[[fallthrough]]; case Directive::CONTINUE: case Directive::DECLINE:
					ctx.recv(aio);
				}
				break;
			}
		}

		template<auto Member_aio, auto Member_ctx, auto Member_delegate, typename T_Self>
		void AsyncRecv_Setup(T_Self *self)
		{
			nng::aio   &aio      = self->*Member_aio;
			//auto       &ctx      = self->*Member_ctx;
			//const auto &delegate = self->*Member_delegate;
			aio = nng::make_aio(&AsyncRecv_Callback_Self<Member_aio, Member_ctx, Member_delegate, T_Self>, self);
		}

		// Utility: an AIO callback suitable for most uses.
		template<auto Member_aio, auto Member_ctx, auto Member_delegate, typename T_Self>
		void AsyncSend_Callback_Self(void *_self)
		{
			auto *self = static_cast<T_Self*>(_self);
			nng::aio   &aio      =  self->*Member_aio;
			auto       &ctx      =  self->*Member_ctx;
			const auto  delegate = (self->*Member_delegate).lock();

			nng::error aioResult = aio.result();

			if (!delegate)
			{
				// Stop sending if there is no delegate.
				return;
			}

			Directive directive =
				(aioResult == nng::error::success)
				? delegate->asyncSend_sent ()
				: delegate->asyncSend_error(aioResult);

			switch (directive)
			{
			default:
			case Directive::AUTO:
				if (aioResult == nng::error::success)
				{
					[[fallthrough]]; case Directive::CONTINUE:
					if (directive.msg())
					{
						aio.set_msg(directive.release_msg());
						ctx.send(aio);
						break;
					}
				}
				[[fallthrough]];

			case Directive::DECLINE:
			case Directive::TERMINATE:
				return;
			}
		}

		template<auto Member_aio, auto Member_ctx, auto Member_delegate, typename T_Self>
		void AsyncSend_Setup(T_Self *self)
		{
			nng::aio   &aio      = self->*Member_aio;
			//auto       &ctx      = self->*Member_ctx;
			//const auto &delegate = self->*Member_delegate;
			aio = nng::make_aio(&AsyncSend_Callback_Self<Member_aio, Member_ctx, Member_delegate, T_Self>, self);
		}
	}


	template<typename T_RecvCtx>
	AsyncRecv::Cycle_<T_RecvCtx>::Cycle_(T_RecvCtx &&_ctx) :
		_recv_ctx(std::move(_ctx))
	{
		detail::AsyncRecv_Setup<
			&Cycle_::_recv_aio,
			&Cycle_::_recv_ctx,
			&Cycle_::_recv_delegate>
			(this);
	}
	template<typename T_RecvCtx>
	AsyncRecv::Cycle_<T_RecvCtx>::~Cycle_()
	{
		recv_stop();
	}

	template<typename T_RecvCtx>
	void AsyncRecv::Cycle_<T_RecvCtx>::recv_start(std::weak_ptr<AsyncRecv> _delegate)
	{
		if (_recv_delegate.lock())
			throw nng::exception(nng::error::busy, "Receive start: already started");

		if (auto delegate = _delegate.lock())
		{
			_recv_delegate = std::move(_delegate);
			delegate->asyncRecv_start(); // May throw
			_recv_ctx.recv(_recv_aio);
		}
	}
	template<typename T_RecvCtx>
	void AsyncRecv::Cycle_<T_RecvCtx>::recv_stop() noexcept
	{
		_recv_aio.stop();
		if (auto delegate = _recv_delegate.lock())
			delegate->asyncRecv_stop(nng::error::success);
	}


	template<typename T_SendCtx>
	AsyncSend::Cycle_<T_SendCtx>::Cycle_(T_SendCtx &&_ctx) :
		_send_ctx(_ctx)
	{
		detail::AsyncSend_Setup<
			&Cycle_::_send_aio,
			&Cycle_::_send_ctx,
			&Cycle_::_send_delegate>
			(this);
	}
	template<typename T_SendCtx>
	AsyncSend::Cycle_<T_SendCtx>::~Cycle_()
	{
		_send_aio.stop();
		if (auto delegate = _send_delegate.lock())
			delegate->asyncSend_stop(nng::error::success);
	}

	template<typename T_SendCtx>
	void AsyncSend::Cycle_<T_SendCtx>::send_init(std::weak_ptr<AsyncSend> _delegate)
	{
		if (_send_delegate.lock())
			throw nng::exception(nng::error::busy, "Send start: already started");

		if (auto delegate = _delegate.lock())
		{
			_send_delegate = std::move(_delegate);
		}
	}

	template<typename T_SendCtx>
	void AsyncSend::Cycle_<T_SendCtx>::send_msg(nng::msg &&msg)
	{
		auto delegate = _send_delegate.lock();

		Directive directive =
			(delegate
				? delegate->asyncSend_msg(std::move(msg))
				: Directive(TERMINATE));
		
		switch (directive)
		{
		default:
		case Directive::AUTO:
			if (directive.msg())
			{
				_send_aio.set_msg(directive.release_msg());
				_send_ctx.send(_send_aio);
				return;
			}
			else
			{
				[[fallthrough]]; case Directive::CONTINUE:
				return;
			}

		case Directive::DECLINE:
			throw nng::exception(nng::error::nospc, "AsyncSend delegate declined the message.");

		case Directive::TERMINATE:
			send_stop();
			throw nng::exception(nng::error::closed, "AsyncSend delegate terminated transmission.");
		}
	}
	template<typename T_SendCtx>
	void AsyncSend::Cycle_<T_SendCtx>::send_stop() noexcept
	{
		_send_aio.stop();
	}
}
