#pragma once


#include <telling/msg_view.h>
#include <telling/msg_writer.h>
#include <telling/service.h>
#include <telling/service_reactor.h>


namespace telling_test
{
	using namespace telling;

	class Test_Responder : public Service
	{
	public:
		class Receiver : public Reactor
		{
		public:
			std::mutex         mtx;
			Test_Responder *service;

			const std::string txt_reply;

		public:
			Receiver(Test_Responder &_service, std::string _reply) : service(&_service), txt_reply(_reply) {}
			~Receiver() {}

			void recv_get(QueryID queryID, const MsgView::Request &request, nng::msg &&msg) final
			{
				MsgView::Request req;
				try
				{
					req = msg;
				}
				catch (MsgException e)
				{
					if (queryID) return DECLINE;
					else         return e.writeReply("ServiceTest_Async");
				}

				std::lock_guard g(mtx);
				if (!service) return;

				if (queryID)
				{
					++service->request_count;

					auto msg = WriteReply();
					msg.writeHeader("Content-Type", "text/plain");
					msg.writeData(service->uri);
					msg.writeData("\r\n");
					msg.writeData(txt_reply);
					return msg.release();
				}
				else
				{
					// Ignore pushed messages
					++service->push_count;
				}
			}
		};

	public:
		Test_Responder(std::string uri, std::string reply) :
			Service(std::make_shared<Receiver>(*this, reply), uri)
		{

		}
		~Test_Responder()
		{

		}

	public:
		volatile size_t
			request_count = 0,
			push_count    = 0;
	};


	class Test_Reflector : public Service
	{
	public:
		class Receiver : public Reactor
		{
		public:
			std::mutex         mtx;
			Test_Reflector *service;

		public:
			Receiver(Test_Reflector &_service) : service(&_service) {}
			~Receiver() {}

			void recv_get(QueryID queryID, const MsgView::Request &request, nng::msg &&msg) final
			{
				MsgView::Request req;
				try
				{
					req = msg;
				}
				catch (MsgException e)
				{
					if (queryID) return DECLINE;
					else         return e.writeReply("ServiceTest_Async");
				}

				std::lock_guard g(mtx);
				if (!service) return;

				// Republish the pushed message
				auto pub = WriteBulletin(service->uri);
				for (auto h : req.headers()) pub.writeHeader(h.name, h.value);
				pub.writeData(req.data());
				service->publish(pub.release());

				if (queryID)
				{
					++service->request_count;

					auto msg = WriteReply(StatusCode::OK);
					return msg.release();
				}
				else
				{
					// No need to respond to push message
					++service->push_count;
				}
			}
		};

	public:
		Test_Reflector(std::string uri) :
			Service(std::make_shared<Receiver>(*this), uri)
		{

		}
		~Test_Reflector()
		{

		}

	public:
		volatile size_t
			request_count = 0,
			push_count    = 0;
	};
}