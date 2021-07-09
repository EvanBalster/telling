#include <telling/msg_writer.h>
#include <telling/server.h>


using namespace telling;



Server::ReqRep::ReqRep() :
	reply_ext  (Role::SERVICE, Pattern::REQ_REP, Socket::RAW),
	request_dvc(Role::CLIENT,  Pattern::REQ_REP, Socket::RAW),
	reply_int  (Role::SERVICE, Pattern::REQ_REP, Socket::RAW),
	rep_send(reply_int.socketView(), ServerResponding{}),
	rep_recv(reply_int.socketView(), ClientRequesting{})
{
	auto server = this->server();
	auto &log = server->log;

	rep_send.send_init (rep_sendQueue.get_weak());
	rep_recv.recv_start(get_weak());

	// Set up device relay
	reply_int  .listen(server->address_internal);
	request_dvc.dial  (server->address_internal);
	thread_device = std::thread(&run_device, this);
}
Server::ReqRep::~ReqRep()
{
	// Stop asynchronous work
	async_lifetime.destroy();

	// Close reply socket (halting further activity)
	reply_int.close();

	// TODO possible deadlock closing these sockets?

	// Close sockets (before joining device)
	request_dvc.close();
	reply_ext.close();

	// Join device.
	thread_device.join();
}

void Server::ReqRep::run_device(ReqRep *reply)
{
	try
	{
		nng::device(reply->reply_ext.socketView(), reply->request_dvc.socketView());
		reply->server()->log << reply->Name() << ": relay thread stopped." << std::endl;
	}
	catch (nng::exception e)
	{
		reply->server()->log << reply->Name() << ": relay thread stopped (" << e.what() << ")" << std::endl;
	}
}


void Server::ReqRep::async_error(ClientRequesting, AsyncError error)
{
	server()->log << Name() << ": Request ingestion error: " << error.what() << std::endl;
}
void Server::ReqRep::async_error(ServiceReplying, AsyncError error)
{
	if (error != nng::error::closed)
	{
		server()->log << Name() << ": Reply ingestion error: " << error.what() << std::endl;
	}
}

void Server::ReqRep::async_recv(ServiceReplying, nng::msg &&msg)
{
	// Multiple instances of this call might be received concurrently.
	//    AsyncSendQueue is mutexed...

	//server()->log << Name() << ": ...sending reply..." << std::endl;

#if 0
	// DIAGNOSTIC
	auto now = std::chrono::steady_clock::now();
	MsgView::Reply reply(msg);

	long long req_time = 0, rep_time = 0;
	for (auto &h : reply.headers())
	{
		if (h.name == "Req-Time") req_time = std::stoll(std::string(h.value));
		if (h.name == "Rep-Time") rep_time = std::stoll(std::string(h.value));
	}
#endif

	// Forward reply to proper client
	try
	{
		rep_send.send_msg(std::move(msg));
	}
	catch (nng::exception e)
	{
		server()->log
			<< Name() << ": could not enqueue reply to client" << std::endl
			<< "\t" << e.what() << std::endl;
	}

#if 0
	// DIAGNOSTIC -- including above code
	if (req_time)
	{
		std::cout << "*REP returning: " << std::chrono::duration_cast<std::chrono::microseconds>
			(now.time_since_epoch() - decltype(now.time_since_epoch())(req_time)).count() << " us" << std::endl;
	}
	std::cout << std::endl;
#endif
}

void Server::ReqRep::async_recv(ClientRequesting, nng::msg &&msg)
{
	auto server = this->server();

	MsgView::Request request;
	try                    {request = msg;}
	catch (MsgException e) {server->log << Name() << ": message exception: " << e.what() << std::endl; return;}

	auto status = server->services.routeRequest(request.uri(), std::move(msg));

	//server->log << Name() << ": routing to `" << request.uri() << "`" << std::endl;

	if (status.isSuccessful())
	{
		// Neat!
	}
	else
	{
		// Log the error.
		server->log << Name() << ": error " << status << " (" << status.reasonPhrase()
			<< ") routing to `" << request.uri() << "`" << std::endl;

		// Reply to client with error message.
		MsgWriter writer;

		// Error reply
		writer.startReply(status);

		switch (status.code)
		{
		case StatusCode::NotFound:
			writer.writeBody()
				<< "No service for URI `" << request.uri() << "`";
			break;
		case StatusCode::ServiceUnavailable:
			writer.writeBody() << "Service exists but forwarding failed.";
			break;
		}


		// Copy routing info
		if (msg) writer.setNNGHeader(msg.header().get());
		else     server->log << "\tPROBLEM: request was discarded, can't reply" << std::endl;

		// Reply, acting as a service.
		async_recv(ServiceReplying{}, writer.release());
	}
}
