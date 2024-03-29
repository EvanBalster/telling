#include <iostream>
#include <iomanip>
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
				cout << std::setw(20) << test.label << ": OK -- ";
				print(view_man);
			}
			else
			{
				cout << std::setw(20) << test.label << ": OK" << endl;
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


void test_system_errors()
{
	int test_errs[34];
	for (int i = 0; i < 32; ++i) test_errs[i] = i;
	test_errs[32] = NNG_EINTERNAL;
	test_errs[33] = NNG_ESYSERR | 0xc0000005;

	cout << "NNG error translation..." << endl;
	for (int errcode : test_errs)
	{
		nng::exception ex(errcode, "test");
		auto def = ex.code().default_error_condition();
		cout
			<< std::right
			<< ex.code().category().name()
			<< std::setw(3) << errcode << ':'
			<< std::setw(8) << def.category().name() << ' '
			<< std::setw(3) << def.value() << "; "
			<< std::setw(35) << ex.what() << " -> "
			<< std::left
			<< std::setw(35) << def.message()
			<< std::right
			<< endl;
	}
}


template<typename Uri_t, typename ExtractMethod>
static void UriParseTest(Uri_t uri, ExtractMethod extract, std::string_view test_name)
{
	std::cout << "Test " << test_name << " ... ";

	std::cout << "\t`" << std::string_view(uri) << "` > ";
	for (size_t i = 0; i < 100; ++i)
	{
		auto frag = (uri.*extract)();
		if (!frag.length()) break;
		std::cout << " `" << frag << "'";
	}
	std::cout << std::endl;
}

static void UriParseTests(std::string_view s)
{
	UriParseTest<UriView>(s, &UriView::pop_front, "UriView::pop_front");
	UriParseTest<Uri    >(s, &Uri    ::pop_front, "    Uri::pop_front");
	UriParseTest<UriView>(s, &UriView::pop_back,  "UriView::pop_back ");
	UriParseTest<Uri    >(s, &Uri    ::pop_back,  "    Uri::pop_back ");
}


int main(int argc, char **argv)
{
	//nng::inproc::register_transport();
	//nng::tcp::register_transport();
	//nng::tls::register_transport();


	//test_system_errors();

	{
		UriParseTests("tetrahedron");
		UriParseTests("tetra/hedron");
		UriParseTests("midi/in_11//sx7/beg/");
		UriParseTests("///bug/in//code?///");
	}

#if 0
	test_message_parsers(false);


	cout << endl;
	cout << "Press ENTER to continue..." << endl;
	std::cin.get();
#endif

	cout << endl;
	cout << "********* HTTP TEST BEGIN *********" << endl;
	try
	{
		HttpClient_Box httpClient(nng::url("https://127.0.0.1:8080"));

		MsgWriter wreq(Http);
		//wreq.startRequest("/mothership/error_query.php");
		wreq.startRequest("/butts.html");
		//wreq.writeHeader("Host", "127.0.0.1");
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


#define CLIENT_AIO 1

	class TestClientHandler : public ClientHandler
	{
	public:
		struct Counter
		{
			size_t n  = 0;
			size_t Ex = 0;

			void add(size_t x) noexcept    {++n; Ex += x;}

			double mean() const noexcept    {return double(Ex)/double(n);}
		};

		Counter
			us_req_rep  = {},
			us_req      = {},
			us_rep      = {},
			us_push_pub = {},
			us_push     = {},
			us_pub      = {};

		size_t total = 0;

		volatile bool service_gone = false;


	public:

		void on_report(nng::msg &&msg)
		{
			long long fin_time = telling_test::MicroTime();

			try
			{
				MsgView::Report bull(msg);

				long long req_time = 0, rep_time = 0;
				for (auto &h : bull.headers())
				{
					if (h.name == "Req-Time") req_time = std::stoll(std::string(h.value));
					if (h.name == "Rep-Time") rep_time = std::stoll(std::string(h.value));
				}

				if ((++total&7) == 0)
				{
					std::cout.put('~');
					std::cout << std::flush;
				}

				//print(bull);
				//cout << "CLI-SUB recv: ";
				//cout << "[" << bull.startLine() << "] `" << bull.bodyString() << "` ";
				//cout << endl;


				if (req_time)
				{
					size_t latency_us = fin_time - req_time;
					us_push_pub.add(latency_us);

					us_push.add(rep_time - req_time);
					us_pub .add(fin_time - rep_time);

					//std::cout << "(RTL:" << latency_us << "us) ";
				}

				//cout << endl;
			}
			catch (MsgException e)
			{
				cout << "CLI-SUB recv: ";
				cout << endl;
				cout << "\t...Error parsing report: " << e.what() << endl;
				cout << "\t...  At location: `" << e.excerpt << '`' << endl;
				cout << endl;
			}
		}

		void on_reply(QueryID id, nng::msg &&msg)
		{
			long long fin_time = telling_test::MicroTime();

			try
			{
				MsgView::Reply reply(msg);

				if (reply.status() == StatusCode::NotFound)
				{
					service_gone = true;
					return;
				}

				if ((++total&7) == 0) std::cout.put('~');


				long long req_time = 0, rep_time = 0;
				for (auto &h : reply.headers())
				{
					if (h.name == "Req-Time") req_time = std::stoll(std::string(h.value));
					if (h.name == "Rep-Time") rep_time = std::stoll(std::string(h.value));
				}

				//print(reply);
				//cout << "CLI-REQ recv: ";

				if (req_time)
				{
					size_t latency_us = fin_time - req_time;
					us_req_rep.add(latency_us);

					us_req.add(rep_time - req_time);
					us_rep.add(fin_time - rep_time);

					//std::cout << "(RTL:" << latency_us << "us) ";
				}

				//cout << "[" << reply.startLine() << "]" << endl; // `" << reply.bodyString() << "`" << endl;
				//print(reply);

				if (reply.status().code == StatusCode::NotFound)
				{
					//cout << "Halting client due to 404 error." << endl;
					//clientKeepGoing = false;
				}
			}
			catch (MsgException e)
			{
				cout << "CLI-REQ recv: ";
				cout << endl;
				cout << "\t...Error parsing report: " << e.what() << endl;
				cout << "\t...  At location: `" << e.excerpt << '`' << endl;
				cout << endl;
			}
		}

		void async_recv(Subscribing sub, nng::msg &&msg) final
		{
			on_report(std::move(msg));
		}

		void async_recv(Requesting req, nng::msg &&msg) final
		{
			on_reply(req.id, std::move(msg));
		}
	};

	edb::life_locked<TestClientHandler> clientHandler;



	{
		cout << "==== Creating client." << endl;

#if CLIENT_AIO
		Client client(clientHandler.weak());
#else
		Client_Box client;
#endif


		cout << "==== Connecting client to server." << endl;

		// Connect these...
		server->open(apiURI);
		client.dial(apiURI);



		std::list<std::future<nng::msg>> pending_requests;

		unsigned clientClock = 0;
		unsigned clientTimeTotal = 0;
		unsigned clientSeq = 0;

		client.subscribe("");


		while (clientTimeTotal < 10000)
		{
#if !CLIENT_AIO
			{
				nng::msg msg;

				while (client.consume(msg))
				{
					clientHandler->on_report(std::move(msg));
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
							clientHandler->on_reply(0, i->get());
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
#endif

			// Every so often...
			std::this_thread::sleep_for(10ms);
			clientTimeTotal += 10;
			clientClock += 10;
			if (clientClock < 10) continue;
			clientClock = 0;

			// New thing happening
			//cout << endl;

			// Compose a request...
			auto msg = WriteRequest(service_uri);
			msg.writeHeader("Content-Type", "text/plain");
			msg.writeHeader("Req-Time", std::to_string(telling_test::MicroTime()));

			// And fire it as push or request.
			switch (++clientSeq & 1)
			{
			case 0:
				// Push
				msg.writeBody() << "I'm getting pushy!";
				//if (!client.requester()->isConnected()) cout << " -- NO CONNECTION";
				client.push(msg.release());
				//cout << "CLI-PUSH send > `" << service_uri << "`";
				//cout << endl;
				break;

			case 1:
				size_t pend_count = 0;
#if CLIENT_AIO
				client.request(msg.release());
#else
				pending_requests.emplace_back(client.request(msg.release()));
				//if (!client.requester()->isConnected()) cout << " -- NO CONNECTION";
				if (pending_requests.size()-1)
				{
					cout << endl << "\t" << pending_requests.size() << " pending ( ";
					auto stats = client.requester()->msgStats();
					if (stats.awaiting_send) cout << stats.awaiting_send << " unsent ";
					if (stats.awaiting_recv) cout << stats.awaiting_recv << " awaiting reply";
					cout << " )";
				}
				cout << endl;
#endif	

				// Make a request
				//cout << "CLI-REQ send > `" << service_uri << "`" << endl;

				break;
			}
		}

		cout << endl;

		cout << "Client LATENCY stats: " << endl
			<< "    Cycles Completed: " << clientHandler->total << endl
			<< "    One-Way    REQ    : " << size_t(clientHandler->us_req.mean()) << " us" << endl
			<< "    One-Way        REP: " << size_t(clientHandler->us_rep.mean()) << " us" << endl
			<< "    Roundtrip  REQ-REP: " << size_t(clientHandler->us_req_rep.mean()) << " us" << endl
			<< "    One-Way   PUSH    : " << size_t(clientHandler->us_push.mean()) << " us" << endl
			<< "    One-Way        PUB: " << size_t(clientHandler->us_pub.mean()) << " us" << endl
			<< "    Roundtrip PUSH-PUB: " << size_t(clientHandler->us_push_pub.mean()) << " us" << endl;

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
