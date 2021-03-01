#include <telling/server.h>
#include <telling/msg_writer.h>


using namespace telling;

using std::endl;


Server::Services::Services() :
	subscribe(subscribe_delegate = std::make_shared<Delegate_Sub>(this))
{
	map.burst_threshold(256);


	// Subscribe to *services and dial into internal address
	subscribe.subscribe("*services");
	subscribe.dial(server()->address_internal);


	// Start management thread
	management.thread = std::thread(&Services::run_management_thread, this);
}

Server::Services::~Services()
{
	if (subscribe_delegate) subscribe_delegate->stop();

	{
		// Shut down management thread
		std::lock_guard g(mtx);
		management.run = false;
		management.cond.notify_one();
	}

	management.thread.join();
}

AsyncOp::Directive Server::Services::receive_error(Delegate_Sub  *, nng::error error)
{
	server()->log << Name() << ": Subscribe ingestion error: " << nng::to_string(error) << std::endl;
	return AsyncOp::AUTO;
}

AsyncOp::Directive Server::Services::received(const MsgView::Bulletin &msg, nng::msg &&_ignored)
{
	auto server = this->server();
	auto &log = server->log;

	/*
		Accept only services message with status 200
	*/

	if (msg.uri != "*services")
	{
		// Don't understand this message
		log << Name() << ": did not recognize URI `" << msg.uri << "`" << endl;
		return AsyncOp::DECLINE;
	}

	if (msg.statusString[0] != '2')
	{
		// Don't understand
		log << Name() << ": ignoring bulletin with status `" << msg.statusString << "`" << endl;
		return AsyncOp::DECLINE;
	}

	/*
		Two-line format:
			Line 1: path prefix (base inproc address)
			Line 2: additional configuration (currently ignored)
	*/
	auto text = msg.dataString();
	const char *pi = text.data(), *pe = pi+text.length();

	auto pathPrefix = detail::ConsumeLine(pi, pe);
	auto configLine = detail::ConsumeLine(pi, pe);

	if (pi != pe)
	{
		log << Name() << ": invalid dial-in." << endl
			<< "\t: prefix `" << pathPrefix << endl
			<< "\t: config `" << configLine << "`" << endl;
		if (pi != pe)
		{
			log << "\t: additional unrecognized data: `"
				<< std::string_view(pi, pe-pi) << "`" << endl;
		}
		return AsyncOp::DECLINE;
	}


	std::lock_guard<std::mutex> g(mtx);


	/*
		Kick off establishment of a new Route.
	*/
	auto baseAddress = HostAddress::Base::InProc(pathPrefix);

	management.route_open.emplace_back(NewRoute{std::string(pathPrefix), baseAddress});
	management.cond.notify_one();

	return AsyncOp::CONTINUE;
}

void Server::Services::disconnected(Route *route)
{
	auto server = this->server();
	auto &log = server->log;

	log << Name() << ": drop `" << route->path << "`..." << endl;

	std::lock_guard<std::mutex> g(mtx);

	auto pos = map.find(route->path);
	if (pos != map.end())
	{
		auto n = map.erase(route->path);
		management.route_close.push_back(route);
		management.cond.notify_one();
	}
	else
	{
		log << "\tWARNING: routing table entry was missing." << endl;
	}
}

void Server::Services::run_management_thread()
{
	std::unique_lock lock(mtx);

	auto server = this->server();
	auto &log = server->log;

	auto &to_open = management.route_open;
	auto &to_close = management.route_close;

	while (management.run)
	{
		while (to_close.size())
		{
			delete to_close.front();
			to_close.pop_front();
		}

		while (to_open.size())
		{
			auto spec = std::move(to_open.front());
			to_open.pop_front();

			/*
				Can't already have this Routing
			*/
			if (map.find(spec.map_uri) != map.end())
			{
				// Consider possibility of race condition if service is torn down and re-established.
				log << Name() << ": already have `" << spec.map_uri << "`" << endl;
				continue;
			}


			/*
				Create service sockets and dial the service.
			*/
			log << Name() << ": enrolling `" << spec.map_uri << "`...";

			try
			{
				Route &sockets = **map.emplace(spec.map_uri, new Route(*server, std::string(spec.map_uri))).first;
				sockets.dial(spec.host);
				log << " ...OK" << endl;

				// Notify service of enrollment.
				auto notify = MsgWriter::Request("*services");
				notify.writeHeader("Content-Type", "text/plain");
				notify.writeData("Enrolled successfully.");
				sockets.sendPush(notify.release());
			}
			catch (nng::exception e)
			{
				log << " ...Failed dialing new route!" << endl
					<< "\t" << e.what() << endl
					<< "\tsource: " << e.who() << endl;
				map.erase(spec.map_uri);
			}
		}

		management.cond.wait(lock);
	}
}



Server::Route::Route(Server &_server, std::string _path) :
	server(_server), path(_path),
	req(*this),
	req_send(req.socketView(), req_sendQueue = std::make_shared<AsyncSendQueue>()),
	req_recv(req.socketView(), server.reply.reply_handler(), true)
{
}
Server::Route::~Route()
{
	if (!halted)
	{
		std::lock_guard<std::mutex> g(mtx);
		halted = true;
	}

	req.close();
	push.close();

	req_send.send_stop();
	req_recv.recv_stop();
}

void Server::Route::dial(const HostAddress::Base &base)
{
	req.dial(base);
	push.dial(base);
}

void Server::Route::RequestRaw::pipeEvent(nng::pipe_view pipe, nng::pipe_ev event)
{
	std::lock_guard<std::mutex> g(route.mtx);

	// Unregister service
	if (event == nng::pipe_ev::rem_post && !route.halted)
	{
		route.halted = true;
		route.server.services.disconnected(&route);
	}
}