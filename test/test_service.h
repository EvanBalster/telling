#pragma once


#include <telling/msg_view.h>
#include <telling/msg_writer.h>
#include <telling/service.h>


namespace telling_test
{
	using namespace telling;

	class ServiceTest_Async : public Service_Async
	{
	public:
		class Receiver : public Service_Async::Handler_Ex
		{
		public:
			std::mutex         mtx;
			ServiceTest_Async *service;

			const std::string txt_reply;

		public:
			Receiver(ServiceTest_Async &_service, std::string _reply)
				: service(&_service),
				txt_reply(_reply)
			{
			}
			~Receiver()
			{
			}

			SendDirective recv(QueryID queryID, nng::msg &&msg) final
			{
				MsgView::Request req;
				try
				{

				}
				catch (MsgException e)
				{
					return e.writeReply("ServiceTest_Async");
				}

				if (queryID)
				{
					auto msg = MsgWriter::Reply();
					msg.writeHeader("Content-Type", "text/plain");
					msg.writeData(txt_reply);
					return msg.release;
				}
			}
		};

	public:
		ServiceTest_Async(std::string uri, std::string reply, std::string heartbeat) :
			Service_Async(std::make_shared<Receiver>(*this, reply)),
			txt_heartbeat(heartbeat)
		{

		}


	private:
		const std::string txt_heartbeat;
	};
}