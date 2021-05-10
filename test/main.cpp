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

#include <telling/http.h>
#include <telling/http_client.h>


using namespace telling;

using std::cout;
using std::endl;


template<class T> void printStartLine(T &t) {cout << "\tStartLine: `" << t.startLine()  << "`" << endl;}
template<class T> void printMethod   (T &t) {cout << "\tMethod:    `" << t.methodString << "` -- parsed as HTTP " << t.method << endl;}
template<class T> void printURI      (T &t) {cout << "\tURI:       `" << t.uri          << "`" << endl;}
template<class T> void printProtocol (T &t) {cout << "\tProtocol:  `" << t.protocol     << "`" << endl;}
template<class T> void printStatus   (T &t) {cout << "\tStatus:    `" << t.statusString << "` -- parsed as HTTP " << t.status() << ' ' << t.status().reasonPhrase() << endl;}
template<class T> void printReason   (T &t) {cout << "\tReason:    `" << t.reason       << "`" << endl;}
void printHeaders(const MsgView &msg)
{
	cout << "\tHeaders... (" << msg.msgHeaders.string.length() << " bytes)" << endl;
	for (auto header : msg.headers())
	{
		cout << "		`" << header.name << "` = `" << header.value << "`" << endl;
	}
}
void printBody(const MsgView &msg)
{
	cout << "/----------------------------------------------------------------\\" << endl;
	cout << std::string_view((char*) msg.data().data(), msg.data().size()) << endl;
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

void print(const MsgView::Bulletin &msg)
{
	cout << "View Bulletin:" << endl;
	printStartLine(msg);
	printURI      (msg);
	printProtocol (msg);
	printStatus   (msg);
	printReason   (msg);
	printHeaders  (msg);
	printBody     (msg);
	cout << endl;
}



void test_message_parsers()
{
	auto string_to_msg = [](const std::string &s)
	{
		nng::msg msg = nng::make_msg(0);
		msg.body().append(nng::view(s.data(), s.length()));
		return msg;
	};



	nng::msg mRequest;
#if TEST_MSG_RAW
	mRequest = string_to_msg(
R"**(PATCH /voices/1 XTELL/0
Content-Type:		application/json 	 

{"attributes": {"slide_mode": "hold"}})**");
#else
	{
		auto msg = MsgWriter::Request("/voices/1", MethodCode::PATCH);
		msg.writeHeader("Content-Type", "application/json");
		msg.writeData(R"*({"attributes": {"slide_mode": "hold"}})*");
		mRequest = msg.release();
	}
#endif

	nng::msg mReply;
#if TEST_MSG_RAW
	mReply = string_to_msg(
R"**(XTELL/0 200 OK
Content-Type:		application/json 	 

{"attributes": {"midi_pitch": 64.729}})**");
#else
	{
		auto msg = MsgWriter::Reply();
		msg.writeHeader("Content-Type", "application/json");
		msg.writeData(R"*({"attributes": {"midi_pitch": 64.729}})*");
		mReply = msg.release();
	}
#endif

	nng::msg mBulletin;
#if TEST_MSG_RAW
	mBulletin = string_to_msg(
R"**(/voices/1  200 
Content-Type:		application/json 	 

{"attributes": {"midi_pitch": 64.729}})**");
#else
	{
		auto msg = MsgWriter::Bulletin("/voices/1");
		msg.writeHeader("Content-Type", "application/json");
		msg.writeData(R"*({"attributes": {"midi_pitch": 64.729}})*");
		mBulletin = msg.release();
	}
#endif

	MsgView::Request  request;
	MsgView::Reply    reply;
	MsgView::Bulletin bulletin;
	try
	{
		request  = MsgView::Request(mRequest);
		cout << "Request parsed..." << endl;
		reply    = MsgView::Reply(mReply);
		cout << "Reply parsed..." << endl;
		bulletin = MsgView::Bulletin(mBulletin);
		cout << "Bulletin parsed..." << endl;
	}
	catch (MsgException e)
	{
		cout << "Error parsing message: " << e.what() << endl;
		cout << "  At location: `" << std::string_view(e.position, e.length) << '`' << endl;
	}

	cout << endl;
	cout << endl;


	print(request);

	print(reply);

	print(bulletin);
}


int main(int argc, char **argv)
{
	nng::inproc::register_transport();
	nng::tcp::register_transport();


	cout << endl;
	cout << "********* HTTP TEST BEGIN *********" << endl;
	try
	{
		HttpClient_Box httpClient(nng::url("http://imitone.com"));

		MsgWriter wreq(Http);
		wreq.startRequest("/mothership/error_query.php");
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


	/*test_message_parsers();


	cout << endl;
	cout << "Press ENTER to continue..." << endl;
	std::cin.get();*/


	using namespace std::chrono_literals;
	

	auto service_thread = [](std::string uri, std::string reply_text, size_t lifetime_ms = 7500) -> void
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
				cout << "SVC-PULL recv: ";
				try
				{
					MsgView::Request req(msg);
					//printStartLine(req);
					//printHeaders(req);
					cout << "[" << req.startLine() << "] `" << req.dataString() << "` -- republishing with note" << endl;

					// Re-publish the pulled message
					auto bulletin = MsgWriter::Bulletin(req.uri);
					for (auto header : req.headers())
					{
						bulletin.writeHeader(header.name, header.value);
					}
					bulletin.writeHeader("X-Republished-By", uri);
					bulletin.writeData(req.data());
					service.publish(bulletin.release());
				}
				catch (MsgException e)
				{
					cout << endl;
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

					auto reply = MsgWriter::Reply();
					reply.writeHeader("Content-Type", "text/plain");
					reply.writeData(reply_text);
					service.respond(reply.release());
				}
				catch (MsgException e)
				{
					cout << endl;
					cout << "\t...Error parsing message: " << e.what() << endl;
					cout << "\t...  At location: `"
						<< std::string_view(e.position, e.length) << '`' << endl;
					cout << endl;

					service.respond(e.writeReply("Test Service"));
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

					cout << endl;
					cout << "PUB send from service (hearbeat)" << endl;

					auto bulletin = MsgWriter::Bulletin(uri);
					bulletin.writeHeader("Content-Type", "text/plain");
					bulletin.writeData("This is a heartbeat message!");
					service.publish(bulletin.release());
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
	auto server = std::make_shared<Server>();


	std::string service_uri = "/voices";

	
	cout << "==== Starting service." << endl;
	std::thread thread(service_thread, service_uri, "There are many voices to choose from.");



	{
		cout << "==== ...wait for service..." << endl;
		std::this_thread::sleep_for(250ms);
		cout << "==== ...wait for service..." << endl;
		std::this_thread::sleep_for(250ms);

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
						MsgView::Bulletin bull(msg);
						//print(bull);
						cout << "[" << bull.startLine() << "] `" << bull.dataString() << "`" << endl;
						cout << endl;
					}
					catch (MsgException e)
					{
						cout << endl;
						cout << "\t...Error parsing bulletin: " << e.what() << endl;
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
								cout << "[" << reply.startLine() << "] `" << reply.dataString() << "`" << endl;
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
								cout << "\t...Error parsing bulletin: " << e.what() << endl;
								cout << "\t...  At location: `"
									<< std::string_view(e.position, e.length) << '`' << endl;
								cout << endl;
							}
						}
						catch (std::exception e)
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
			auto msg = MsgWriter::Request(service_uri);
			msg.writeHeader("Content-Type", "text/plain");

			// And fire it as push or request.
			switch (++clientSeq & 1)
			{
			case 0:
				// Push
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
	thread.join();
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