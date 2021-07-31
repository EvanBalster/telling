#include <thread>
#include <iostream>

#include "test_service.h"


using namespace telling;
using std::cout;
using std::endl;
using namespace std::literals;


void telling_test::Service_PollingThread::run()
{
	unsigned timer = 0;
	unsigned timerTotal = 0;

	size_t recvCount = 0;

	std::this_thread::sleep_for(2500ms);

	cout << "==== Creating service." << endl;
	{
	Service_Box service(uri);

	while (timerTotal < lifetime_ms)
	{
		nng::msg msg;

		while (service.pull(msg))
		{
			++recvCount;
			try
			{
				MsgView::Request req(msg);
				//printStartLine(req);
				//printHeaders(req);

				// Re-publish the pulled message
				auto report = WriteReport(req.uri());
				for (auto header : req.headers())
				{
					report.writeHeader(header.name, header.value);
				}
				report.writeHeader("X-Republished-By", uri);
				report.writeBody() << req.body() << " (republished)";
				service.publish(report.release());
			}
			catch (MsgException e)
			{
				cout << "SVC-PULL recv" << endl;
				cout << "\t...Error parsing message: " << e.what() << endl;
				cout << "\t...  At location: `" << e.excerpt << '`' << endl;
				cout << endl;
			}
		}

		while (service.receive(msg))
		{
			++recvCount;
			//cout << "SVC-REP recv: ";
			try
			{
				MsgView::Request req(msg);
				//printStartLine(req);
				//printHeaders(req);
				//cout << "[" << req.startLine() << "] `" << req.dataString() << "`" << endl;

				auto reply = WriteReply();
				reply.writeHeader("Content-Type", "text/plain");
				reply.writeBody() << reply_text;
				service.respond(reply.release());
			}
			catch (MsgException e)
			{
				cout << "SVC-REP recv" << endl;
				cout << "\t...Error parsing message: " << e.what() << endl;
				cout << "\t...  At location: `" << e.excerpt << '`' << endl;
				cout << endl;

				service.respond(e.replyWithError("Test Service"));
			}
		}

		std::this_thread::sleep_for(10ms);
		timer += 10;
		timerTotal += 10;

		if (recvCount == 0)
		{
			if (timer > 100)
			{
				timer = 0;
				//service.broadcastServiceRegistration();
			}
		}
		else
		{
			if (timer > 1000)
			{
				timer = 0;

				auto report = WriteReport(uri);
				report.writeHeader("Content-Type", "text/plain");
				report.writeBody() << "This is a heartbeat message!";
				service.publish(report.release());
			}
		}
	}

	cout << "==== Destroying service." << endl;
	}
	cout << "==== Destroyed service..." << endl;
	//std::this_thread::sleep_for(2000ms);
	//cout << "==== Destroyed service, a little while ago." << endl;
	cout << endl;
};