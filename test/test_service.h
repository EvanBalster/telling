#pragma once


#include <chrono>
#include <life_lock.hpp>

#include <telling/msg_writer.h>
#include <telling/service.h>
#include <telling/service_reactor.h>
#include <telling/service_reactor.h>


namespace telling_test
{
	using namespace telling;


	inline long long MicroTime()
	{
		return std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now().time_since_epoch()
			).count();
	}


	struct Service_PollingThread
	{
		std::string uri;
		std::string reply_text;
		size_t      lifetime_ms = 7500;

		void run();
	};

	class Service_AIO : public Service
	{
	public:
		class Handler : public Reactor
		{
		public:
			Service_AIO *service;

			const std::string txt_reply;

			size_t lifetime_ms;
			long long created_microTime;

		public:
			Handler(Service_AIO &_service, std::string _reply, size_t _lifetime_ms) : 
				Reactor(_service.uri),
				service(&_service), txt_reply(_reply),
				lifetime_ms(_lifetime_ms),
				created_microTime(MicroTime()) {}
			~Handler() {}

			Methods allowed(UriView) const noexcept override
			{
				return MethodCode::GET;
			}

			void async_get(Query query, Msg::Request &&request) final
			{
				long long recv_time = telling_test::MicroTime();

				if ((recv_time - created_microTime) > 1000*((long long) lifetime_ms))
				{
					// Pretend I don't exist
					throw status_exceptions::NotFound();
				}

				long long req_time = 0;
				for (auto &h : request.headers())
				{
					if (h.name == "Req-Time")
						req_time = std::stoll(std::string(h.value));
				}
				
				if (query.reply)
				{
					++service->request_count;

					auto msg = WriteReply();
					msg.writeHeader("Content-Type", "text/plain");
					if (req_time) msg.writeHeader("Req-Time", std::to_string(req_time));
					msg.writeHeader("Rep-Time", std::to_string(recv_time));
					msg.writeBody()
						<< service->uri
						<< "\r\n"
						<< txt_reply;
					service->respondTo(query.id, msg.release());
				}
				else
				{
					// Rebroadcast
					++service->push_count;

					// Re-publish the pulled message
					auto report = WriteReport(request.uri());
					for (auto header : request.headers())
					{
						report.writeHeader(header.name, header.value);
					}
					report.writeHeader("Rep-Time", std::to_string(recv_time));
					report.writeHeader("X-Republished-By", service->uri);
					report.writeBody() << request.body() << " (republished)";
					service->publish(report.release());
				}

				if (req_time)
				{
					//std::cout << "GET Latency to server: " << std::chrono::duration_cast<std::chrono::microseconds>
					//	(now.time_since_epoch() - decltype(now.time_since_epoch())(req_time)).count() << " us" << std::endl;
				}
			}
		};

	public:
		Service_AIO(std::string uri, std::string reply, size_t lifetime_ms = 7500) :
			Service(uri),
			handler(*this, reply, lifetime_ms)
		{
			initialize(handler.weak());
		}
		~Service_AIO()
		{
		}

	public:
		edb::life_locked<Handler> handler;

		volatile size_t
			request_count = 0,
			push_count    = 0;
	};

#if 0
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
					else         return e.replyWithError("ServiceTest_Async");
				}

				std::lock_guard g(mtx);
				if (!service) return;

				// Republish the pushed message
				auto pub = WriteReport(service->uri);
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
#endif
}
