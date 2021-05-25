#include <telling/msg_writer.h>
#include <telling/server.h>


using namespace telling;



Server::Reply::Reply() :
	reply_ext  (Role::SERVICE, Pattern::REQ_REP, Socket::RAW),
	request_dvc(Role::CLIENT,  Pattern::REQ_REP, Socket::RAW),
	reply_int  (Role::SERVICE, Pattern::REQ_REP, Socket::RAW),
	rep_send(reply_int.socketView(), rep_sendQueue    = std::make_shared<AsyncSendQueue>()),
	rep_recv(reply_int.socketView(), delegate_request = std::make_shared<Delegate_Request>(this)),
	delegate_reply(std::make_shared<Delegate_Reply>(this))
{
	auto server = this->server();
	auto &log = server->log;

	// Set up device relay
	reply_int  .listen(server->address_internal);
	request_dvc.dial  (server->address_internal);
	thread_device = std::thread(&run_device, this);
}
Server::Reply::~Reply()
{
	// Disconnect delegates
	delegate_reply  ->stop();
	delegate_request->stop();

	// Close reply socket (halting further activity)
	reply_int.close();

	// TODO possible deadlock closing these sockets?

	// Close sockets (before joining device)
	request_dvc.close();
	reply_ext.close();

	// Join device.
	thread_device.join();
}

void Server::Reply::run_device(Reply *reply)
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


AsyncOp::Directive Server::Reply::receive_error(Delegate_Request*, nng::error error)
{
	server()->log << Name() << ": Request ingestion error: " << nng::to_string(error) << std::endl;
	return AsyncOp::AUTO;
}
AsyncOp::Directive Server::Reply::receive_error(Delegate_Reply  *, nng::error error)
{
	if (error != nng::error::closed)
	{
		server()->log << Name() << ": Reply ingestion error: " << nng::to_string(error) << std::endl;
	}
	return AsyncOp::AUTO;
}

AsyncOp::Directive Server::Reply::received(const MsgView::Reply &reply, nng::msg &&msg)
{
	// Only one reply will be received at a time so this is thread-safe.

	server()->log << Name() << ": ...sending reply with status " << reply.status() << std::endl;

	// Forward reply to proper 
	try
	{
		rep_send.send_msg(std::move(msg));
		return AsyncOp::CONTINUE;
	}
	catch (nng::exception e)
	{
		server()->log
			<< Name() << ": could not enqueue reply to client" << std::endl
			<< "\t" << e.what() << std::endl;
		return AsyncOp::DECLINE;
	}
}

AsyncOp::Directive Server::Reply::received(const MsgView::Request &request, nng::msg &&msg)
{
	auto server = this->server();

	auto status = server->services.routeRequest(request.uri(), std::move(msg));

	//server->log << Name() << ": routing to `" << request.uri << "`" << std::endl;

	if (status.isSuccessful())
	{
		return AsyncOp::CONTINUE;
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

		// Copy routing info
		if (msg) writer.setNNGHeader(msg.header().get());
		else     server->log << "\tPROBLEM: request was discarded, can't reply" << std::endl;

		switch (status.code)
		{
		case StatusCode::NotFound:
			writer.writeData("No service for URI `");
			writer.writeData(request.uri());
			writer.writeData("`");
			break;
		case StatusCode::ServiceUnavailable:
			writer.writeData("Service exists but forwarding failed.");
			break;
		}

		delegate_reply->asyncRecv_msg(writer.release());

		return AsyncOp::DECLINE;
	}
}
