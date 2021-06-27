#pragma once


#include "async.h"


namespace telling
{
	/*
		Optional base class for AIO receiver that calls an AsyncRecv object.
	*/
	template<typename Tag, class T_RecvCtx = nng::socket_view>
	class AsyncRecvLoop
	{
	public:
		using Handler = AsyncRecv<Tag>;

	public:
		AsyncRecvLoop(T_RecvCtx &&_ctx, Tag);
		~AsyncRecvLoop();

		// Start/stop receiving.  Start may throw exceptions on failure.
		void recv_start(std::weak_ptr<Handler>);
		void recv_stop ()  noexcept;

		T_RecvCtx       &recv_ctx()       noexcept    {return _ctx;}
		const T_RecvCtx &recv_ctx() const noexcept    {return _ctx;}

		std::weak_ptr<Handler> recv_handler() const    {return _handler;}

	private:
		Tag                    _tag; // TODO [[no_unique_address]]
		nng::aio               _aio;
		T_RecvCtx              _ctx;
		std::weak_ptr<Handler> _handler;
	};

	/*
		Optional base class for AIO sender that calls an AsyncSend object.
	*/
	template<typename Tag, class T_SendCtx = nng::socket_view>
	class AsyncSendLoop
	{
	public:
		using Handler = AsyncSend<Tag>;

	public:
		AsyncSendLoop(T_SendCtx &&_ctx, Tag);
		~AsyncSendLoop();

		/*
			send_msg may throw an exception if the handler refuses.
				send_stop halts sending.
		*/
		void send_init(std::weak_ptr<Handler>);
		void send_msg (nng::msg &&msg);
		void send_stop()              noexcept;

		T_SendCtx       &send_ctx()       noexcept    {return _ctx;}
		const T_SendCtx &send_ctx() const noexcept    {return _ctx;}

		std::weak_ptr<Handler> send_handler() const    {return _handler;}

	private:
		Tag                    _tag; // TODO [[no_unique_address]]
		nng::aio               _aio;
		T_SendCtx              _ctx;
		std::weak_ptr<Handler> _handler;
	};


	/*
		Implementation stuff follows...
	*/
	namespace detail
	{
		template<
			auto Member_tag,
			auto Member_aio,
			auto Member_ctx,
			auto Member_handler, typename T_Self>
		void AsyncRecv_Callback_Self(void *_self)
		{
			auto *self = static_cast<T_Self*>(_self);
			auto       &tag      =  self->*Member_tag;
			nng::aio   &aio      =  self->*Member_aio;
			auto       &ctx      =  self->*Member_ctx;
			const auto  handler = (self->*Member_handler).lock();

			nng::error aioResult = aio.result();

			if (!handler)
			{
				// Stop receiving if there is no handler.
				if (aioResult == nng::error::success) aio.release_msg();
				return;
			}

			bool receiveAnother = false;

			switch (aioResult)
			{
			case nng::error::success:
				// Receive and continue
				handler->async_recv(tag, aio.release_msg());
				break;

			case nng::error::timedout:
				// Note error and continue
				handler->async_error(tag, aioResult);
				break;

			case nng::error::canceled:
			default:
				// Cease receiving
				handler->async_error(tag, aioResult);
				handler->async_stop(tag, aioResult);
				return;
			}

			// Receive another message.
			ctx.recv(aio);
		}

		// Utility: an AIO callback suitable for most uses.
		template<
			auto Member_tag,
			auto Member_aio,
			auto Member_ctx,
			auto Member_handler,
			typename T_Self>
		void AsyncSend_Callback_Self(void *_self)
		{
			auto *self = static_cast<T_Self*>(_self);
			auto        tag      =  self->*Member_tag;
			nng::aio   &aio      =  self->*Member_aio;
			auto       &ctx      =  self->*Member_ctx;
			const auto  handler = (self->*Member_handler).lock();

			nng::error aioResult = aio.result();

			if (!handler)
			{
				// Stop sending if there is no handler.
				return;
			}

			nng::msg nextMsg;

			switch (aioResult)
			{
			case nng::error::success:
				tag.send.setDest(nextMsg);
				handler->async_sent (tag);
				break;

			default:
			case nng::error::timedout:
				tag.send.setDest(nextMsg);
				handler->async_error(tag, aioResult);
				break;

			case nng::error::canceled:
				// Cannot send another message
				handler->async_error(tag, aioResult);
				return;
			}

			if (nextMsg)
			{
				aio.set_msg(std::move(nextMsg));
				ctx.send(aio);
			}
		}
	}


	template<typename Tag, typename T_RecvCtx>
	AsyncRecvLoop<Tag, T_RecvCtx>::AsyncRecvLoop(T_RecvCtx &&_ctx, Tag tag) :
		_tag(tag), _ctx(std::move(_ctx))
	{
		_aio = nng::make_aio(&detail::AsyncRecv_Callback_Self<
			&AsyncRecvLoop::_tag,
			&AsyncRecvLoop::_aio,
			&AsyncRecvLoop::_ctx,
			&AsyncRecvLoop::_handler,
			AsyncRecvLoop>, this);
	}
	template<typename Tag, typename T_RecvCtx>
	AsyncRecvLoop<Tag, T_RecvCtx>::~AsyncRecvLoop()
	{
		recv_stop();
	}

	template<typename Tag, typename T_RecvCtx>
	void AsyncRecvLoop<Tag, T_RecvCtx>::recv_start(std::weak_ptr<Handler> new_handler)
	{
		if (_handler.lock())
			throw nng::exception(nng::error::busy, "Receive start: already started");

		auto handler = new_handler.lock();
		if (!handler)
			throw nng::exception(nng::error::closed, "Receive start: handler has expired");

		_handler = std::move(new_handler);
		handler->async_start(_tag); // May throw
		_ctx.recv(_aio);
	}
	template<typename Tag, typename T_RecvCtx>
	void AsyncRecvLoop<Tag, T_RecvCtx>::recv_stop() noexcept
	{
		_aio.stop();
		if (auto handler = _handler.lock())
			handler->async_stop(_tag, nng::error::success);
	}


	template<typename Tag, typename T_SendCtx>
	AsyncSendLoop<Tag, T_SendCtx>::AsyncSendLoop(T_SendCtx &&_ctx, Tag tag) :
		_tag(tag), _ctx(_ctx)
	{
		_aio = nng::make_aio(&detail::AsyncSend_Callback_Self<
			&AsyncSendLoop::_tag,
			&AsyncSendLoop::_aio,
			&AsyncSendLoop::_ctx,
			&AsyncSendLoop::_handler,
			AsyncSendLoop>, this);
	}
	template<typename Tag, typename T_SendCtx>
	AsyncSendLoop<Tag, T_SendCtx>::~AsyncSendLoop()
	{
		_aio.stop();
		if (auto handler = _handler.lock())
			handler->async_stop(_tag, nng::error::success);
	}

	template<typename Tag, typename T_SendCtx>
	void AsyncSendLoop<Tag, T_SendCtx>::send_init(std::weak_ptr<Handler> new_handler)
	{
		if (_handler.lock())
			throw nng::exception(nng::error::busy, "Send init: already started");

		auto handler = new_handler.lock();
		if (!handler)
			throw nng::exception(nng::error::closed, "Send init: handler has expired");

		_handler = std::move(new_handler);
		handler->async_start(_tag);
	}

	template<typename Tag, typename T_SendCtx>
	void AsyncSendLoop<Tag, T_SendCtx>::send_msg(nng::msg &&msg)
	{
		auto handler = _handler.lock();

		if (!handler) throw nng::exception(nng::error::exist,
			"AsyncSendLoop::send_msg: handler does not exist");

		auto tag = _tag;
		tag.send.setDest(msg); // Aliasing...
		handler->async_prep(_tag, msg);

		if (msg)
		{
			_aio.set_msg(std::move(msg));
			_ctx.send(_aio);
		}
	}
	template<typename Tag, typename T_SendCtx>
	void AsyncSendLoop<Tag, T_SendCtx>::send_stop() noexcept
	{
		_aio.stop();
	}
}
