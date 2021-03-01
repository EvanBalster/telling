#pragma once


#include <memory>
#include <nngpp/aio.h>


namespace telling
{
	class AsyncOp
	{
	public:
		enum DIRECTIVE
		{
			// Default behavior based on status.
			AUTO      = 0,

			// Continue ongoing communications.
			CONTINUE  = 1,

			// Refrain from processing a message.  Communications continue.
			DECLINE   = 2,

			// (Re-)Initiate communications.  Often synonymous with Continue.
			INITIATE  = 3,

			// Stop communications, canceling any which are in progress.
			TERMINATE = 4,
		};

		struct Directive
		{
			const DIRECTIVE directive;

			Directive(DIRECTIVE d)     : directive(d) {}

			operator DIRECTIVE() const noexcept    {return directive;}
		};

		struct SendDirective
		{
			const DIRECTIVE directive;
			nng::msg        sendMsg;

			SendDirective(DIRECTIVE d)       : directive(d) {}
			SendDirective(nng::msg &&msg)    : directive(INITIATE), sendMsg(std::move(msg)) {}
		};

		struct QueryDirective
		{
			const Directive directive;
			nng::aio        aio;
			nng::msg        sendMsg;
		};
	};


	/*
		Callback interface for receiving messages.
			Used for PULL and SUBscribe protocols.
	*/
	class AsyncRecv : public AsyncOp
	{
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
		virtual Directive asyncRecv_error(nng::error status)     {return TERMINATE;}
		virtual void      asyncRecv_stop (nng::error status)     {}


	public:
		/*
			Optional base class for AIO receiver that calls an AsyncRecv object.
		*/
		template<class T_RecvCtx>
		class Operator
		{
		public:
			Operator(T_RecvCtx &&_ctx, std::shared_ptr<AsyncRecv> _delegate, bool begin = true);
			~Operator();

			// Start/stop receiving.  Start may throw exceptions on failure.
			void recv_start();
			void recv_stop()  noexcept;

			T_RecvCtx       &recv_ctx()       noexcept    {return _recv_ctx;}
			const T_RecvCtx &recv_ctx() const noexcept    {return _recv_ctx;}

			std::shared_ptr<AsyncRecv> recv_delegate() const    {return _recv_delegate;}

		private:
			nng::aio                   _recv_aio;
			T_RecvCtx                  _recv_ctx;
			std::shared_ptr<AsyncRecv> _recv_delegate;
		};
	};

	/*
		Callback interface for sending messages.
			Used for PUSH and PUBlish protocols.
		
		After a successful send, we may send another message by returning
			CONTINUE and filling in sendMsg.
	*/
	class AsyncSend : public AsyncOp
	{
	public:
		virtual ~AsyncSend() {}

		/*
			asyncSend has concurrency responsibilities.
				"msg" should enqueue the message if another is sending or return it if not.
				"sent" notifies that one message has finished sending.
		*/
		virtual SendDirective asyncSend_msg  (nng::msg &&msg)       = 0;
		virtual SendDirective asyncSend_sent ()                     = 0;
		virtual SendDirective asyncSend_error(nng::error status)    {return TERMINATE;}
		virtual void          asyncSend_stop (nng::error status)    {}


	public:
		/*
			Optional base class for AIO sender that calls an AsyncSend object.
		*/
		template<class T_SendCtx>
		class Operator
		{
		public:
			Operator(T_SendCtx &&_ctx, std::shared_ptr<AsyncSend> _delegate);
			~Operator();

			/*
				send_msg may throw an exception if the delegate refuses.
				send_stop halts sending.
			*/
			void send_msg(nng::msg &&msg);
			void send_stop()              noexcept;

			T_SendCtx       &send_ctx()       noexcept    {return _send_ctx;}
			const T_SendCtx &send_ctx() const noexcept    {return _send_ctx;}

			std::shared_ptr<AsyncSend> send_delegate() const    {return _send_delegate;}

		private:
			nng::aio                   _send_aio;
			T_SendCtx                  _send_ctx;
			std::shared_ptr<AsyncSend> _send_delegate;
		};
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
			nng::aio   &aio      = self->*Member_aio;
			auto       &ctx      = self->*Member_ctx;
			const auto &delegate = self->*Member_delegate;

			nng::error aioResult = aio.result();
			AsyncOp::Directive directive =
				(aioResult == nng::error::success)
				? delegate->asyncRecv_msg  (aio.release_msg())
				: delegate->asyncRecv_error(aioResult);

			switch (directive.directive)
			{
			default:
			case AsyncOp::AUTO:
				if (aioResult != nng::error::success)
				{
					[[fallthrough]]; case AsyncOp::TERMINATE:
					delegate->asyncRecv_stop(aioResult);
					return;
				}
				else
				{
					[[fallthrough]]; case AsyncOp::CONTINUE: case AsyncOp::INITIATE: case AsyncOp::DECLINE:
					ctx.recv(aio);
				}
				break;
			}
		}

		template<auto Member_aio, auto Member_ctx, auto Member_delegate, typename T_Self>
		void AsyncRecv_Setup(T_Self *self)
		{
			nng::aio   &aio      = self->*Member_aio;
			auto       &ctx      = self->*Member_ctx;
			const auto &delegate = self->*Member_delegate;
			aio = nng::make_aio(&AsyncRecv_Callback_Self<Member_aio, Member_ctx, Member_delegate, T_Self>, self);
		}

		// Utility: an AIO callback suitable for most uses.
		template<auto Member_aio, auto Member_ctx, auto Member_delegate, typename T_Self>
		void AsyncSend_Callback_Self(void *_self)
		{
			auto *self = static_cast<T_Self*>(_self);
			nng::aio   &aio      = self->*Member_aio;
			auto       &ctx      = self->*Member_ctx;
			const auto &delegate = self->*Member_delegate;

			nng::error aioResult = aio.result();
			AsyncOp::SendDirective directive =
				(aioResult == nng::error::success)
				? delegate->asyncSend_sent ()
				: delegate->asyncSend_error(aioResult);

			switch (directive.directive)
			{
			default:
			case AsyncOp::AUTO:
				if (aioResult == nng::error::success)
				{
					[[fallthrough]]; case AsyncOp::CONTINUE: case AsyncOp::INITIATE:
					if (directive.sendMsg)
					{
						aio.set_msg(std::move(directive.sendMsg));
						ctx.send(aio);
						break;
					}
				}
				[[fallthrough]];

			case AsyncOp::DECLINE:
			case AsyncOp::TERMINATE:
				return;
			}
		}

		template<auto Member_aio, auto Member_ctx, auto Member_delegate, typename T_Self>
		void AsyncSend_Setup(T_Self *self)
		{
			nng::aio   &aio      = self->*Member_aio;
			auto       &ctx      = self->*Member_ctx;
			const auto &delegate = self->*Member_delegate;
			aio = nng::make_aio(&AsyncSend_Callback_Self<Member_aio, Member_ctx, Member_delegate, T_Self>, self);
		}
	}


	template<typename T_RecvCtx>
	AsyncRecv::Operator<T_RecvCtx>::Operator(T_RecvCtx &&_ctx, std::shared_ptr<AsyncRecv> _delegate, bool autoStart) :
		_recv_ctx(std::move(_ctx)), _recv_delegate(std::move(_delegate))
	{
		detail::AsyncRecv_Setup<
			&Operator::_recv_aio,
			&Operator::_recv_ctx,
			&Operator::_recv_delegate>
			(this);
		if (autoStart)
			recv_start();
	}
	template<typename T_RecvCtx>
	AsyncRecv::Operator<T_RecvCtx>::~Operator()
	{
		recv_stop();
	}

	template<typename T_RecvCtx>
	void AsyncRecv::Operator<T_RecvCtx>::recv_start()
	{
		_recv_delegate->asyncRecv_start(); // May throw

		_recv_ctx.recv(_recv_aio);
	}
	template<typename T_RecvCtx>
	void AsyncRecv::Operator<T_RecvCtx>::recv_stop() noexcept
	{
		_recv_aio.stop();
		_recv_delegate->asyncRecv_stop(nng::error::success);
	}


	template<typename T_SendCtx>
	AsyncSend::Operator<T_SendCtx>::Operator(T_SendCtx &&_ctx, std::shared_ptr<AsyncSend> _delegate) :
		_send_ctx(_ctx), _send_delegate(std::move(_delegate))
	{
		detail::AsyncSend_Setup<
			&Operator::_send_aio,
			&Operator::_send_ctx,
			&Operator::_send_delegate>
			(this);
	}
	template<typename T_SendCtx>
	AsyncSend::Operator<T_SendCtx>::~Operator()
	{
		_send_aio.stop();
		_send_delegate->asyncSend_stop(nng::error::success);
	}

	template<typename T_SendCtx>
	void AsyncSend::Operator<T_SendCtx>::send_msg(nng::msg &&msg)
	{
		SendDirective directive = _send_delegate->asyncSend_msg(std::move(msg));
		switch (directive.directive)
		{
		default:
		case AUTO:
			if (directive.sendMsg)
			{
				[[fallthrough]]; case INITIATE:
				_send_aio.set_msg(std::move(directive.sendMsg));
				_send_ctx.send(_send_aio);
				return;
			}
			else
			{
				[[fallthrough]]; case CONTINUE:
				return;
			}

		case DECLINE:
			throw nng::exception(nng::error::nospc, "AsyncSend delegate declined the message.");

		case TERMINATE:
			send_stop();
			throw nng::exception(nng::error::closed, "AsyncSend delegate terminated transmission.");
		}
	}
	template<typename T_SendCtx>
	void AsyncSend::Operator<T_SendCtx>::send_stop() noexcept
	{
		_send_aio.stop();
	}
}