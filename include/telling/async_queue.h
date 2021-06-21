#pragma once


#include "io_queue.h"
#include "async.h"


namespace telling
{
	template<typename Tag>
	class AsyncRecvQueue : public AsyncRecv<Tag>, public RecvQueueMtx
	{
	public:
		AsyncRecvQueue() {}
		~AsyncRecvQueue() override {}

		void async_recv(Tag, nng::msg &&recvMsg) override
		{
			RecvQueueMtx::push(std::move(recvMsg));
		}
	};

	template<typename Tag>
	class AsyncSendQueue : public AsyncSend<Tag>, public SendQueueMtx
	{
	public:
		AsyncSendQueue() {}
		~AsyncSendQueue() override {}

		void async_prep(Tag tag, nng::msg &msg) override
		{
			if (SendQueueMtx::produce(std::move(msg))) return;
			tag.send(std::move(msg));
		}

		void async_sent(Tag tag) override
		{
			nng::msg next;
			if (SendQueueMtx::consume(next)) tag.send(std::move(next));
		}
	};
}