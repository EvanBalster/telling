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

#include "test_service.h"


using namespace telling;

using std::cout;
using std::endl;


template<class T> void printStartLine(T &t) {cout << "\tStartLine: `" << t.startLine()      << "`" << endl;}
template<class T> void printMethod   (T &t) {cout << "\tMethod:    `" << t.methodString()   << "` -- interpret " << t.method() << endl;}
template<class T> void printURI      (T &t) {cout << "\tURI:       `" << t.uri()            << "`" << endl;}
template<class T> void printProtocol (T &t) {cout << "\tProtocol:  `" << t.protocolString() << "` -- interpret " << t.protocol() << endl;}
template<class T> void printStatus   (T &t) {cout << "\tStatus:    `" << t.statusString()   << "` -- interpret " << t.status() << ' ' << t.status().reasonPhrase() << endl;}
template<class T> void printReason   (T &t) {cout << "\tReason:    `" << t.reason()         << "`" << endl;}
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

void test_message_parsers(bool should_print)
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
		{Msg::TYPE::REQUEST,  "Tiny Request", "GET /a"            "\n\n"},
		{Msg::TYPE::REQUEST,   "Min Request", "GET "              "\n\n"},
		{Msg::TYPE::REPLY,   "Small Reply",          "Tell/0 404" "\n\n"},
		{Msg::TYPE::REPLY,     "Min Reply",                " 404" "\n\n"},
		{Msg::TYPE::REPORT,  "Small Report",      "/a Tell/0 201" "\n\n"},
		{Msg::TYPE::REPORT,   "Tiny Report",      "/a Tell/0"     "\n\n"},
		{Msg::TYPE::REPORT,    "Min Report",      "/a"            "\n\n"},
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
		tests.emplace_back(Msg::TYPE::REQUEST, "Gen. Request", msg.release());
	}

	{
		auto msg = WriteReply();
		msg.writeHeader("Content-Type", "application/json");
		msg.writeBody() << R"*({"attributes": {"midi_pitch": 64.729}})*";
		tests.emplace_back(Msg::TYPE::REPLY, "Gen. Reply", msg.release());
	}

	{
		auto msg = WriteReport("/voices/1");
		msg.writeHeader("Content-Type", "application/json");
		msg.writeBody() << R"*({"attributes": {"midi_pitch": 64.729}})*";
		tests.emplace_back(Msg::TYPE::REPORT, "Gen. Report", msg.release());
	}

	cout << "=== Begin message I/O tests..." << endl;
	size_t issue_count = 0;
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
				cout << "***" << endl << endl;
				++issue_count;
			}

			if (should_print)
			{
				cout << test.label << ": OK -- ";
				print(view_man);
			}
			else
			{
				cout << test.label << ": OK" << endl;
			}
		}
		catch (MsgException &e)
		{
			cout << "*** Parse exception" << std::endl;
			cout << "\tin case: " << test.label << std::endl;
			cout << "\tError: " << e.what() << std::endl;
			cout << "\tLocation: `" << e.excerpt << '`' << endl;
			cout << "***" << endl << endl;
			++issue_count;
		}
	}
	cout << "=== Completed message I/O tests with " << issue_count << " issues..." << endl;

	cout << endl;
	cout << endl;
}


int main(int argc, char **argv)
{
	nng::inproc::register_transport();
	nng::tcp::register_transport();
	nng::tls::register_transport();


	test_message_parsers(false);


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

#define SERVICE_AIO 1

#if SERVICE_AIO
	auto service_aio = new telling_test::Service_AIO(
		service_uri,
		"There are many voices to choose from."
	);
#else
	telling_test::Service_PollingThread sPoll =
	{
		service_uri,
		"There are many voices to choose from."
	};
	std::thread service_thread(&decltype(sPoll)::run, &sPoll);
#endif



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


		while (clientTimeTotal < 12'500)
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
						cout << "\t...  At location: `" << e.excerpt << '`' << endl;
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
								//print(reply);

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
								cout << "\t...  At location: `" << e.excerpt << '`' << endl;
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
	
#if SERVICE_AIO
	cout << "==== Stop service (polling thread)." << endl;
	delete service_aio;
	cout << "==== Stopped service (polling thread)." << endl;
	cout << endl;
#else
	cout << "==== Stop service (polling thread)." << endl;
	service_thread.join();
	cout << "==== Stopped service (polling thread)." << endl;
	cout << endl;
#endif

	cout << "==== Destroying server." << endl;
	server = nullptr;
	cout << "==== Destroyed server." << endl;
	cout << endl;

	cout << "Press ENTER to conclude..." << endl;
	std::cin.get();

	return 0;
}