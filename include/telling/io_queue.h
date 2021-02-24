#pragma once

#include <deque>
#include <mutex>
#include <nngpp/msg.h>
#include <nngpp/aio.h>
#include <nngpp/ctx.h>


namespace telling
{
	/*
		A message queue which also manages some asynchronous message handler.
			When producing 
	*/
	template<typename T>
	class RecvQueueMtx_
	{
	public:
		/*
			Enqueue a message.
		*/
		void push(T &&msg)
		{
			std::lock_guard<std::mutex> g(mtx);
			deq.emplace_back(std::move(msg));
		}

		/*
			Dequeue a message, if possible.
				Returns FALSE on failure due to an empty queue.
		*/
		bool pull(T &msg)
		{
			std::lock_guard<std::mutex> g(mtx);
			if (deq.empty()) return false;
			msg = std::move(deq.front());
			deq.pop_front();
			return true;
		}

		/*
			Purge all messages from the queue.
		*/
		void clear() noexcept
		{
			std::lock_guard<std::mutex> g(mtx);
			deq.clear();
		}

		/*
			Check if the queue is empty.
		*/
		bool empty() const noexcept
		{
			std::lock_guard<std::mutex> g(mtx);
			return deq.empty();
		}
		
	private:
		std::mutex    mtx;
		std::deque<T> deq;
	};

	using RecvQueueMtx = RecvQueueMtx_<nng::msg>;


	/*
		A message queue which helps to manage transmission.
	*/
	template<typename T>
	class SendQueueMtx_
	{
	public:
		/*
			Enqueue a message and check whether send is "busy"
				Returns TRUE and moves the message if the queue is non-empty.
				Returns FALSE and does not move the message if the queue is empty.
		*/
		bool produce(T &&msg)
		{
			std::lock_guard<std::mutex> g(mtx);
			if (_busy)
			{
				deq.emplace_back(std::move(msg));
				return true;
			}
			else
			{
				// Now busy.
				_busy = true;
				return false;
			}
		}

		/*
			Dequeue a message, if possible.
				If the queue is empty, returns FALSE and cease being busy.
		*/
		bool consume(T &msg)
		{
			std::lock_guard<std::mutex> g(mtx);
			if (deq.empty())
			{
				// No longer busy
				_busy = false;
				return false;
			}
			msg = std::move(deq.front());
			deq.pop_front();
			return true;
		}

		// Consume and discard all messages.
		void clear() noexcept
		{
			std::lock_guard<std::mutex> g(mtx);
			deq.clear();
		}

		bool busy() const noexcept
		{
			std::lock_guard<std::mutex> g(mtx);
			return _busy;
		}

		bool empty() const noexcept
		{
			std::lock_guard<std::mutex> g(mtx);
			return deq.empty();
		}
		
	private:
		std::mutex    mtx;
		std::deque<T> deq;
		bool          _busy = false;
	};

	using SendQueueMtx = SendQueueMtx_<nng::msg>;


	/*
	*/
	template<typename CtxView>
	struct QueuedRecv_
	{
	public:
		QueuedRecv_()                {}
		QueuedRecv_(CtxView _ctx)    {open(_ctx);}
		~QueuedRecv_() noexcept      {close();}

		void open(CtxView _ctx)
		{
			if (aio) close();
			ctx = _ctx;
			aio = nng::make_aio(&_aioReceived, this);
			ctx.recv(aio);
		}
		void close() noexcept
		{
			if (aio) aio.stop();
			aio = nng::aio();
			ctx = CtxView();
		}

		bool pull(nng::msg &msg)
		{
			return queue.pull(msg);
		}

	private:
		nng::aio      aio;
		CtxView       ctx;
		RecvQueueMtx  queue;

		static void _aioReceived(void *_self)
		{
			auto self = static_cast<QueuedRecv_*>(_self);

			// Halt?
			switch (self->aio.result())
			{
			case nng::error::success:  break;
			case nng::error::canceled:
			case nng::error::timedout:
			default:                   return;
			}

			// Add message to queue.
			self->queue.push(self->aio.release_msg());

			// Receive another message.
			self->ctx.recv(self->aio);
		}
	};

	using QueuedRecv    = QueuedRecv_<nng::socket_view>;
	using QueuedRecvCtx = QueuedRecv_<nng::ctx_view>;


	/*
	*/
	template<typename CtxView>
	struct QueuedSend_
	{
	public:
		QueuedSend_()                {}
		QueuedSend_(CtxView _ctx)    {open(_ctx);}
		~QueuedSend_() noexcept      {close();}

		void open(CtxView _ctx)
		{
			if (aio) close();
			ctx = _ctx;
			aio = nng::make_aio(&_aioSent, this);
		}
		void close() noexcept
		{
			if (aio) aio.stop();
			aio = nng::aio();
			ctx = CtxView();
			queue.clear();
		}

		void send(nng::msg &&msg)
		{
			if (!ctx)
				throw nng::exception(nng::error::closed, "QueuedSend is not open.");

			if (queue.produce(std::move(msg)))
			{
				// AIO processing continues
			}
			else
			{
				// Immediately claim the message
				aio.set_msg(std::move(msg));
				ctx.send(aio);
			}
		}

	private:
		nng::aio      aio;
		CtxView       ctx;
		SendQueueMtx queue;

		static void _aioSent(void *_self)
		{
			auto self = static_cast<QueuedSend_*>(_self);

			// Halt?
			switch (self->aio.result())
			{
			case nng::error::success:  break;
			case nng::error::canceled:
			case nng::error::timedout:
			default:                   return;
			}

			{
				nng::msg next;
				if (self->queue.consume(next))
				{
					// Transmit another message
					self->aio.set_msg(std::move(next));
					self->ctx.send(self->aio);
				}
				else
				{
					// AIO sequence completes
				}
			}
		}
	};

	using QueuedSend    = QueuedSend_<nng::socket_view>;
	using QueuedSendCtx = QueuedSend_<nng::ctx_view>;
}
