#include <telling/server.h>


using namespace telling;



Server::PushPull::PushPull()
{
	auto server = this->server();
	auto &log = server->log;

	pull.initialize(get_weak());
}
Server::PushPull::~PushPull()
{
	// Stop asynchronous work
	async_lifetime.destroy();

	pull.disconnectAll();
}


void Server::PushPull::async_error(Pulling, AsyncError error)
{
	server()->log << Name() << ": ingestion error: " << error.what() << std::endl;
}

void Server::PushPull::async_recv(Pulling, nng::msg &&msg)
{
	auto server = this->server();

	MsgView::Request request;
	try                    {request = msg;}
	catch (MsgException e) {server->log << Name() << ": message exception: " << e.what() << std::endl; return;}

	auto status = server->services.routePush(request.uri(), std::move(msg));

	server->log << Name() << ": pushing to URI `" << request.uri() << "`" << std::endl;

	if (status.isSuccessful())
	{
		// Neat
	}
	else
	{
		// Log the error.
		server->log << Name() << ": error " << status << " (" << status.reasonPhrase()
			<< ") routing to `" << request.uri() << "`" << std::endl;

		// There's no opportunity to reply, so the failed message is discarded.
	}
}
