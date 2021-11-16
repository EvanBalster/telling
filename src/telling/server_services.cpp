#include <telling/server.h>
#include <telling/msg_writer.h>


using namespace telling;

using std::endl;


Server::Services::Services(Server &_server) :
	server(_server)
{
	register_reply.initialize(get_weak());

	register_reply.socket()->setPipeHandler(get_weak());

	map.burst_threshold(256);


	// Responders may dial in and request registration
	register_reply.listen(server.address_register);

	// Publish service events
	publish_events.dial(server.address_internal);



	// Start management thread
	management.thread = std::thread(&Services::run_management_thread, this);
}

Server::Services::~Services()
{
	// Stop asynchronous work
	async_lifetime.destroy();

	{
		// Shut down management thread
		std::lock_guard g(mtx);
		management.run = false;
		management.cond.notify_one();
	}

	management.thread.join();
}


void Server::Services::async_sent(Replying rep)
{
}
void Server::Services::async_error(Replying rep, AsyncError status)
{
	server.log << Name()
		<< ": Registration Responder error: " << nng::to_string(status) << std::endl;
}

void Server::Services::async_recv(Replying rep, nng::msg &&_msg)
{
	auto &log = server.log;

	auto queryID = rep.id;


	nng::msg owned_msg = std::move(_msg);
	MsgView::Request msg;
	try
	{
		msg = MsgView::Request(owned_msg);
	}
	catch (MsgException e)
	{
		log << Name() << ": message parse exception: " << e.what() << std::endl;

		rep.send(e.replyWithError("Service Registry"));
		return;
	}

	/*
		Accept only services message with status 200
	*/

	if (msg.uri() != "*services")
	{
		// Don't understand this message
		log << Name() << ": did not recognize URI `" << msg.uri() << "`" << endl;
	}

	/*
		Two-line format:
			Line 1: path prefix (base inproc address)
			Line 2: additional configuration (currently ignored)
	*/
	auto text = msg.bodyString();
	const char *pi = text.data(), *pe = pi+text.length();

	auto pathPrefix = detail::ConsumeLine(pi, pe);
	auto configLine = detail::ConsumeLine(pi, pe);

	// Body parse failure
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

		auto writer = WriteReply(HttpStatus::Code::BadRequest);
		writer.writeBody() << "Malformed Registration Request Body.";
		rep.send(writer.release());
		return;
	}


	std::lock_guard<std::mutex> g(mtx);

	// Add to registration map
	auto pipeID = owned_msg.get_pipe().get().id;
	registrationMap.emplace(pipeID, pathPrefix);

	/*
		Kick off establishment of a new Route.
	*/
	auto baseAddress = HostAddress::Base::InProc(pathPrefix);

	management.route_open.emplace_back(NewRoute{queryID, pipeID, std::string(pathPrefix), baseAddress});
	management.cond.notify_one();
}

void Server::Services::pipeEvent(Socket *socket, nng::pipe_view pipe, nng::pipe_ev event)
{
	if (event != nng::pipe_ev::rem_post)
	{
		// We are only interested in "disconnect" events for now.
		return;
	}
	
	auto &log = server.log;

	auto pipeID = pipe.get().id;

	log << Name() << ": disconnect #" << pipeID << " ";

	std::lock_guard<std::mutex> g(mtx);

	std::string path;
	{
		auto pipe_pos = registrationMap.find(pipeID);
		if (pipe_pos == registrationMap.end())
		{
			log << "... not registered." << endl;
			registrationMap.erase(pipe_pos);
			return;
		}
		log << "`" << pipe_pos->second << "`... " << endl;;

		path = std::move(pipe_pos->second);
		registrationMap.erase(pipe_pos);
	}

	auto pos = map.find(path);
	if (pos != map.end())
	{
		Route *route = *pos;
		auto n = map.erase(pos);
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

	auto &log = server.log;

	auto &to_open = management.route_open;
	auto &to_close = management.route_close;

	while (management.run)
	{
		while (to_close.size())
		{
			auto route = to_close.front();
			to_close.pop_front();
			server.publish.subscribe.disconnect(route->path);

			// Publish existence of new service?
			auto report = WriteReport("*services", StatusCode::Gone);
			report.writeBody() << route->path;
			publish_events.publish(report.release());

			delete route;
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

				// Remove pipe
				registrationMap.erase(spec.pipeID);

				// Failed dialing... service unavailable
				auto notify = WriteReply(StatusCode::Conflict);
				notify.writeHeader("Content-Type", "text/plain");
				notify.writeBody()
					<< std::string(spec.map_uri)
					<< "\nThis URI is already registered.";
				register_reply.respondTo(spec.queryID, notify.release());

				continue;
			}


			/*
				Create service sockets and dial the service.
			*/
			log << Name() << ": registering service `" << spec.map_uri << "`..." << endl;

			try
			{
				Route &sockets = **map.emplace(spec.map_uri, new Route(server, std::string(spec.map_uri))).first;

				// Connect pub-sub
				server.publish.subscribe.dial(spec.host);


				sockets.dial(spec.host);

				log << " ...OK" << endl;
			}
			catch (nng::exception e)
			{
				log << " ...Failed dialing new route!" << endl
					<< "\t" << e.what() << endl
					<< "\tsource: " << e.who() << endl;
				map.erase(spec.map_uri);
				
				// Remove pipe
				registrationMap.erase(spec.pipeID);

				// Failed dialing... service unavailable
				auto notify = WriteReply(StatusCode::ServiceUnavailable);
				notify.writeHeader("Content-Type", "text/plain");
				notify.writeBody()
					<< std::string(spec.host.base)
					<< "\nCould not dial specified service URI.";
				register_reply.respondTo(spec.queryID, notify.release());

				continue;
			}

			// Notify service of successful enrollment.
			auto notify = WriteReply();
			notify.writeHeader("Content-Type", "text/plain");
			notify.writeBody()
				<< spec.map_uri
				<< "\nEnrolled with this URI.";
			register_reply.respondTo(spec.queryID, notify.release());

			// Publish existence of new service
			auto report = WriteReport("*services", StatusCode::Created);
			report.writeBody() << spec.map_uri;
			publish_events.publish(report.release());
		}

		management.cond.wait(lock);
	}
}



Server::Route::Route(Server &_server, std::string _path) :
	server(_server), path(_path),
	req(*this),
	req_send_to_service  (req.socketView(), ClientRequesting{}),
	req_recv_from_service(req.socketView(), ServiceReplying{})
{
	req_send_to_service.send_init(req_sendQueue.weak());

	// Route replies from services
	req_recv_from_service.recv_start(server.reply.get_weak());
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

	req_send_to_service.send_stop();
	req_recv_from_service.recv_stop();
}

void Server::Route::dial(const HostAddress::Base &base)
{
	req.dial(base);
	push.dial(base);
}

void Server::Route::sendPush   (nng::msg &&msg)
{
	std::lock_guard<std::mutex> g(mtx);
	push.push(std::move(msg));
}
void Server::Route::sendRequest(nng::msg &&msg) 
{
	std::lock_guard<std::mutex> g(mtx);
	req_send_to_service.send_msg(std::move(msg));
}
