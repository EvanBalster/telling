#include <telling/server.h>
#include <telling/msg_writer.h>


using namespace telling;

using std::endl;


class Server::Services::RegisterResponder : public AsyncRespond
{
public:
	std::mutex mtx;
	Services *services;

	RegisterResponder(Services *_services) : services(_services) {}


	SendDirective asyncRespond_recv(QueryID qid, nng::msg &&query)  final
	{
		std::lock_guard gSelf(mtx);
		if (!services) return TERMINATE;

		return services->registerRequest(qid, std::move(query));
	}
	void asyncRespond_done(QueryID qid) final
	{
		std::lock_guard gSelf(mtx);
		if (!services) return;

		//std::lock_guard g(services->mtx);
	}
	Directive asyncRespond_error(QueryID qid, nng::error status) final
	{
		std::lock_guard gSelf(mtx);
		if (!services) return TERMINATE;

		services->server()->log << services->Name()
			<< ": Registration Responder error: " << nng::to_string(status) << std::endl;
		return AsyncOp::AUTO;
	}

	void pipeEvent(Socket *socket, nng::pipe_view pipe, nng::pipe_ev event) final
	{
		std::lock_guard gSelf(mtx);
		if (!services) return;

		if (event == nng::pipe_ev::rem_post)
		{
			services->registerExpired(pipe);
		}
	}

	void stop()
	{
		std::lock_guard gSelf(mtx);
		services = nullptr;
	}
};


Server::Services::Services() :
	register_responder(std::make_shared<RegisterResponder>(this)),
	register_reply(register_responder)
{
	map.burst_threshold(256);


	// Responders may dial in and request registration
	register_reply.listen(server()->address_register);

	// Publish service events
	publish_events.listen(server()->address_register);



	// Start management thread
	management.thread = std::thread(&Services::run_management_thread, this);
}

Server::Services::~Services()
{
	if (register_responder) static_cast<RegisterResponder*>(&*register_responder)->stop();

	{
		// Shut down management thread
		std::lock_guard g(mtx);
		management.run = false;
		management.cond.notify_one();
	}

	management.thread.join();
}

AsyncOp::SendDirective Server::Services::registerRequest(QueryID queryID, nng::msg &&_msg)
{
	auto server = this->server();
	auto &log = server->log;


	nng::msg owned_msg = std::move(_msg);
	MsgView::Request msg;
	try
	{
		msg = MsgView::Request(owned_msg);
	}
	catch (MsgException e)
	{
		log << Name() << ": message parse exception: " << e.what() << std::endl;

		return e.writeReply("Service Registration Request Handler");
	}

	/*
		Accept only services message with status 200
	*/

	if (msg.uri != "*services")
	{
		// Don't understand this message
		log << Name() << ": did not recognize URI `" << msg.uri << "`" << endl;
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

		auto writer = MsgWriter::Reply(HttpStatus::Code::BadRequest);
		writer.writeData("Malformed Registration Request Body.");
		return writer.release();
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

	return AsyncOp::CONTINUE;
}

void Server::Services::registerExpired(nng::pipe_view pipe)
{
	auto server = this->server();
	auto &log = server->log;

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

	auto server = this->server();
	auto &log = server->log;

	auto &to_open = management.route_open;
	auto &to_close = management.route_close;

	while (management.run)
	{
		while (to_close.size())
		{
			auto route = to_close.front();
			to_close.pop_front();
			server->publish.subscribe.disconnect(route->path);

			// Publish existence of new service?
			auto bulletin = MsgWriter::Bulletin("*services", StatusCode::Gone);
			bulletin.writeData(route->path);
			publish_events.publish(bulletin.release());

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
				auto notify = MsgWriter::Reply(StatusCode::Conflict);
				notify.writeHeader("Content-Type", "text/plain");
				notify.writeData(std::string(spec.map_uri));
				notify.writeData("\nThis URI is already registered.");
				register_reply.respondTo(spec.queryID, notify.release());

				continue;
			}


			/*
				Create service sockets and dial the service.
			*/
			log << Name() << ": enrolling `" << spec.map_uri << "`...";

			try
			{
				Route &sockets = **map.emplace(spec.map_uri, new Route(*server, std::string(spec.map_uri))).first;

				// Connect pub-sub
				server->publish.subscribe.dial(spec.host);


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
				auto notify = MsgWriter::Reply(StatusCode::ServiceUnavailable);
				notify.writeHeader("Content-Type", "text/plain");
				notify.writeData(std::string(spec.host.base));
				notify.writeData("\nCould not dial specified service URI.");
				register_reply.respondTo(spec.queryID, notify.release());

				continue;
			}

			// Notify service of successful enrollment.
			auto notify = MsgWriter::Reply();
			notify.writeHeader("Content-Type", "text/plain");
			notify.writeData(spec.map_uri);
			notify.writeData("\nEnrolled with this URI.");
			register_reply.respondTo(spec.queryID, notify.release());

			// Publish existence of new service?
			auto bulletin = MsgWriter::Bulletin("*services", StatusCode::Created);
			bulletin.writeData(spec.map_uri);
			publish_events.publish(bulletin.release());
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

void Server::Route::sendPush   (nng::msg &&msg)
{
	std::lock_guard<std::mutex> g(mtx);
	push.push(std::move(msg));
}
void Server::Route::sendRequest(nng::msg &&msg) 
{
	std::lock_guard<std::mutex> g(mtx);
	req_send.send_msg(std::move(msg));
}
