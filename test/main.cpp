#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <list>

//#include <telling/server.h>
//#include <telling/client.h>

#include <nngpp/transport/inproc.h>
#include <nngpp/transport/tcp.h>

#include <telling/msg_writer.h>
#include <telling/msg_view.h>

#include <telling/server.h>
#include <telling/service.h>
#include <telling/client.h>

#include <telling/http_client.h>


using namespace telling;

using std::cout;
using std::endl;


template<class T> void printStartLine(T &t) {cout << "\tStartLine: `" << t.startLine()    << "`" << endl;}
template<class T> void printMethod   (T &t) {cout << "\tMethod:    `" << t.methodString() << "` -- parsed as HTTP " << t.method() << endl;}
template<class T> void printURI      (T &t) {cout << "\tURI:       `" << t.uri()          << "`" << endl;}
template<class T> void printProtocol (T &t) {cout << "\tProtocol:  `" << t.protocol()     << "`" << endl;}
template<class T> void printStatus   (T &t) {cout << "\tStatus:    `" << t.statusString() << "` -- parsed as HTTP " << t.status() << ' ' << t.status().reasonPhrase() << endl;}
template<class T> void printReason   (T &t) {cout << "\tReason:    `" << t.reason()       << "`" << endl;}
void printHeaders(const MsgView &msg)
{
	cout << "\tHeaders... (" << msg.headers().length() << " bytes)" << endl;
	for (auto header : msg.headers())
	{
		cout << "		`" << header.name << "` = `" << header.value << "`" << endl;
	}
}
void printBody(const MsgView &msg)
{
	cout << "/----------------------------------------------------------------\\ " << msg.bodySize() << endl;
	cout << std::string_view((char*) msg.bodyData<char>(), msg.bodySize()) << endl;
	cout << "\\----------------------------------------------------------------/" << endl;
}

void print(const MsgView::Request &msg)
{
	cout << "View Request:" << endl;
	printStartLine(msg);
	printMethod   (msg);
	printURI      (msg);
	printProtocol (msg);
	printHeaders  (msg);
	printBody     (msg);
	cout << endl;
}

void print(const MsgView::Reply &msg)
{
	cout << "View Reply:" << endl;
	printStartLine(msg);
	printProtocol (msg);
	printStatus   (msg);
	printReason   (msg);
	printHeaders  (msg);
	printBody     (msg);
	cout << endl;
}

void print(const MsgView::Report &msg)
{
	cout << "View Report:" << endl;
	printStartLine(msg);
	printURI      (msg);
	printProtocol (msg);
	printStatus   (msg);
	printReason   (msg);
	printHeaders  (msg);
	printBody     (msg);
	cout << endl;
}

void print(const MsgView &msg)
{
	switch (msg.msgType())
	{
	case Msg::TYPE::REQUEST: print(static_cast<const MsgView::Request&>(msg)); break;
	case Msg::TYPE::REPORT : print(static_cast<const MsgView::Report &>(msg)); break;
	case Msg::TYPE::REPLY  : print(static_cast<const MsgView::Reply  &>(msg)); break;
	default:
		std::cout << "View unknown-type message:" << endl;
		printStartLine(msg);
		printMethod   (msg);
		printURI      (msg);
		printProtocol (msg);
		printStatus   (msg);
		printReason   (msg);
		printHeaders  (msg);
		printBody     (msg);
		cout << endl;
	}
}

std::ostream &operator<<(std::ostream &o, Msg::TYPE t)
{
	switch (t)
	{
	case Msg::TYPE::REQUEST: return o << "REQUEST";
	case Msg::TYPE::REPORT: return o << "REPORT";
	case Msg::TYPE::REPLY: return o << "REPLY";
	default:              return o << "UNKNOWN";
	}
}

void test_message_parsers()
{
	using namespace std::literals;

	auto string_to_msg = [](std::string_view s)
	{
		nng::msg msg = nng::make_msg(0);
		msg.body().append(nng::view(s.data(), s.length()));
		return msg;
	};


	struct MsgRaw
	{
		Msg::TYPE        type;
		std::string_view label;
		std::string_view text;
	};

	MsgRaw raws[] =
	{
		{Msg::TYPE::REQUEST, "Full Request",
		R"**(PATCH /voices/1 Tell/0
Content-Type:		application/json 	 

{"attributes": {"slide_mode": "hold"}})**"},
		{Msg::TYPE::REPLY, "Full Reply",
R"**(Tell/0 200 OK
Content-Type:		application/json 	 

{"attributes": {"midi_pitch": 64.729}})**"},
		{Msg::TYPE::REPORT,  "Full Report",
R"**(/voices/1 Tell/0 201 Created
Content-Type:		application/json 	 

{"attributes": {"midi_pitch": 64.729}})**"},
		{Msg::TYPE::REQUEST, "Min Request", "GET /a Tell/0" "\n\n"},
		{Msg::TYPE::REPLY, "Min Reply", "Tell/0 404 Not Found" "\n\n"},
		{Msg::TYPE::REPORT,  "Min Report", "/a Tell/0 201 Created" "\n\n"},
	};

	struct MsgTest
	{
		Msg::TYPE   type;
		std::string label;
		nng::msg    msg;

		MsgTest(Msg::TYPE t, std::string_view l, nng::msg &&m) : type(t), label(l), msg(std::move(m)) {}
	};

	std::list<MsgTest> tests;

	for (auto &raw : raws)
	{
		tests.emplace_back(raw.type, std::string(raw.label) + " (raw)"s, string_to_msg(raw.text));
	}

	{
		auto msg = WriteRequest("/voices/1", MethodCode::PATCH);
		msg.writeHeader("Content-Type", "application/json");
		msg.writeBody() << R"*({"attributes": {"slide_mode": "hold"}})*";
		tests.emplace_back(Msg::TYPE::REQUEST, "Full Request", msg.release());
	}

	{
		auto msg = WriteReply();
		msg.writeHeader("Content-Type", "application/json");
		msg.writeBody() << R"*({"attributes": {"midi_pitch": 64.729}})*";
		tests.emplace_back(Msg::TYPE::REPLY, "Generated Reply", msg.release());
	}

	{
		auto msg = WriteReport("/voices/1");
		msg.writeHeader("Content-Type", "application/json");
		msg.writeBody() << R"*({"attributes": {"midi_pitch": 64.729}})*";
		tests.emplace_back(Msg::TYPE::REPORT, "Generated Report", msg.release());
	}

	for (auto &test : tests)
	{
		// ...
		try
		{
			MsgView view_man(test.msg, test.type);

			MsgView view_auto(test.msg);

			if (view_auto.msgType() != test.type)
			{
				cout << "*** Detected wrong message type" << std::endl;
				cout << "\tin case: " << test.label << std::endl;
				cout << "\texpected " << test.type << ", got " << view_auto.msgType() << std::endl;
			}

			print(view_man);
		}
		catch (MsgException &e)
		{
			cout << "*** Parse exception" << std::endl;
			cout << "\tin case: " << test.label << std::endl;
			cout << "\tError: " << e.what() << std::endl;
			cout << "\tLocation: `" << std::string_view(e.position, e.length) << '`' << endl;
		}
	}

	cout << endl;
	cout << endl;
}


int main(int argc, char **argv)
{
	nng::inproc::register_transport();
	nng::tcp::register_transport();
	nng::tls::register_transport();


	test_message_parsers();


	cout << endl;
	cout << "Press ENTER to continue..." << endl;
	std::cin.get();


	cout << endl;
	cout << "********* HTTP TEST BEGIN *********" << endl;
	try
	{
		HttpClient_Box httpClient(nng::url("https://imitone.com"));

		MsgWriter wreq(Http);
		//wreq.startRequest("/mothership/error_query.php");
		wreq.startRequest("/activate.php");
		wreq.writeHeader("Host", "imitone.com");
		//wreq.writeHeader_Length();

		nng::msg reqMsg = wreq.release();

		{
			std::ofstream out("http_request.txt", std::ios::binary);
			out.write(reqMsg.body().get().data<char>(), reqMsg.body().size());
		}

		cout << "HTTP REQUEST COMPOSED:" << endl;
		print(MsgView::Request(reqMsg));

		auto replyPromise = httpClient.request(std::move(reqMsg));

		cout << "HTTP AWAIT REPLY..." << endl;
		auto repMsg = replyPromise.get();

		print(MsgView::Reply(repMsg));
	}
	catch (nng::exception e)
	{
		cout << "Exception in HTTP request: " << e.what() << endl
			<< "\tcontext: " << e.who() << endl;
	}
	cout << "********* HTTP TEST END *********" << endl;
	cout << endl;


	using namespace std::chrono_literals;
	

	auto run_service_thread = [](std::string uri, std::string reply_text, size_t lifetime_ms = 7500) -> void
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
					cout << "\t...  At location: `"
						<< std::string_view(e.position, e.length) << '`' << endl;
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
					cout << "\t...  At location: `"
						<< std::string_view(e.position, e.length) << '`' << endl;
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
		std::this_thread::sleep_for(2000ms);
		cout << "==== Destroyed service, a little while ago." << endl;
		cout << endl;
	};



	auto apiURI = HostAddress::Base::InProc("telling_test");


	// Server needs no thread

	cout << "==== Creating server." << endl;
	auto server = std::make_shared<Server>(&std::cout);


	std::string service_uri = "/voices";



	/*cout << "==== ...wait for service..." << endl;
	std::this_thread::sleep_for(250ms);
	cout << "==== ...wait for service..." << endl;
	std::this_thread::sleep_for(250ms);*/


	cout << "==== Starting service." << endl;
	std::thread service_thread(run_service_thread, service_uri, "There are many voices to choose from.");



	{
		cout << "==== Creating client." << endl;

		Client_Box client;


		cout << "==== Connecting client to server." << endl;

		// Connect these...
		server->open(apiURI);
		client.dial(apiURI);



		std::list<std::future<nng::msg>> pending_requests;

		unsigned clientClock = 0;
		unsigned clientTimeTotal = 0;
		unsigned clientSeq = 0;

		client.subscribe("");


		bool clientKeepGoing = true;


		while (clientKeepGoing && clientTimeTotal < 12'500)
		{
			{
				nng::msg msg;

				while (client.consume(msg))
				{
					cout << "CLI-SUB recv: ";
					try
					{
						MsgView::Report bull(msg);
						//print(bull);
						cout << "[" << bull.startLine() << "] `" << bull.bodyString() << "`" << endl;
						cout << endl;
					}
					catch (MsgException e)
					{
						cout << endl;
						cout << "\t...Error parsing report: " << e.what() << endl;
						cout << "\t...  At location: `"
							<< std::string_view(e.position, e.length) << '`' << endl;
						cout << endl;
					}
				}

				// Get replies
				for (auto i = pending_requests.begin(); i != pending_requests.end(); )
				{
					switch (i->wait_for(0ms))
					{
					case std::future_status::deferred:
					case std::future_status::timeout:
						++i;
						break;
					case std::future_status::ready:
						try
						{
							cout << "CLI-REQ recv: ";
							msg = i->get();

							try
							{
								MsgView::Reply reply(msg);
								//print(reply);
								cout << "[" << reply.startLine() << "] `" << reply.bodyString() << "`" << endl;
								cout << endl;

								if (reply.status().code == StatusCode::NotFound)
								{
									//cout << "Halting client due to 404 error." << endl;
									//clientKeepGoing = false;
								}
							}
							catch (MsgException e)
							{
								cout << endl;
								cout << "\t...Error parsing report: " << e.what() << endl;
								cout << "\t...  At location: `"
									<< std::string_view(e.position, e.length) << '`' << endl;
								cout << endl;
							}
						}
						catch (nng::exception &e)
						{
							cout << "\t...failed with NNG exception: " << e.what() << " in " << e.who() << endl;
							cout << endl;
						}
						catch (std::exception &e)
						{
							cout << "\t...failed with exception: " << e.what() << endl;
							cout << endl;
						}
						pending_requests.erase(i++);
						break;
					}
				}
			}

			// Every so often...
			std::this_thread::sleep_for(10ms);
			clientTimeTotal += 10;
			clientClock += 10;
			if (clientClock < 500) continue;
			clientClock = 0;

			// New thing happening
			cout << endl;

			// Compose a request...
			auto msg = WriteRequest(service_uri);
			msg.writeHeader("Content-Type", "text/plain");

			// And fire it as push or request.
			switch (++clientSeq & 1)
			{
			case 0:
				// Push
				msg.writeBody() << "I'm getting pushy!";
				cout << "CLI-PUSH send > `" << service_uri << "`";
				if (!client.requester()->isConnected()) cout << " -- NO CONNECTION";
				cout << endl;
				client.push(msg.release());
				break;

			case 1:
				// Make a request
				cout << "CLI-REQ send > `" << service_uri << "`";
				if (!client.requester()->isConnected()) cout << " -- NO CONNECTION";
				if (pending_requests.size())
				{
					cout << endl << "\t" << pending_requests.size() << " pending ( ";
					auto stats = client.requester()->msgStats();
					if (stats.awaiting_send) cout << stats.awaiting_send << " unsent ";
					if (stats.awaiting_recv) cout << stats.awaiting_recv << " awaiting reply";
					cout << " )";
				}
				cout << endl;
				pending_requests.emplace_back(client.request(msg.release()));
				break;
			}
		}

		cout << "==== Destroying client." << endl;
	}
	cout << "==== Destroyed client." << endl;
	cout << endl;
	
	cout << "==== Join service thread." << endl;
	service_thread.join();
	cout << "==== Joined service thread." << endl;
	cout << endl;

	cout << "==== Destroying server." << endl;
	server = nullptr;
	cout << "==== Destroyed server." << endl;
	cout << endl;

	cout << "Press ENTER to conclude..." << endl;
	std::cin.get();

	return 0;
}