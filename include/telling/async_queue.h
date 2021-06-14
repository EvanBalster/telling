#pragma once


#include "io_queue.h"
#include "async.h"


namespace telling
{
	class AsyncRecvQueue : public AsyncRecv, public RecvQueueMtx
	{
	public:
		AsyncRecvQueue() {}
		~AsyncRecvQueue() override {}

		Directive asyncRecv_msg(nng::msg &&recvMsg) override
		{
			RecvQueueMtx::push(std::move(recvMsg));
			return AUTO;
		}
	};


	class AsyncSendQueue : public AsyncSend, public SendQueueMtx
	{
	public:
		AsyncSendQueue() {}
		~AsyncSendQueue() override {}

		Directive asyncSend_msg(nng::msg &&msg) override
		{
			if (SendQueueMtx::produce(std::move(msg))) return CONTINUE;
			else return std::move(msg);
		}

		Directive asyncSend_sent() override
		{
			nng::msg next;
			if (SendQueueMtx::consume(next)) return next;
			else                             return DECLINE;
		}
	};
}