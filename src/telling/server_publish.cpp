#include <nngpp/protocol/sub0.h>
#include <telling/server.h>


using namespace telling;



Server::PubSub::PubSub()
{
	auto server = this->server();
	auto &log = server->log;

	// The subscriber is a relay, and accepts all topics.
	subscribe.subscribe("");

	// Relay service events to internal modules (dial-in mvechanism)
	subscribe.listen(server->address_internal);

	subscribe.initialize(get_weak());
}
Server::PubSub::~PubSub()
{
	// Stop asynchronous work
	async_lifetime.destroy();

	subscribe.disconnectAll();
	publish.disconnectAll();
}


void Server::PubSub::async_error(Subscribing, AsyncError error)
{
	server()->log << Name() << ": ingestion error: " << error.what() << std::endl;
}

void Server::PubSub::async_recv(Subscribing, nng::msg &&msg)
{
	// No mutex needed; this AIO is the only sender.

	auto server = this->server();
	auto &log = server->log;

	MsgView::Report report;
	try                    {report = msg;}
	catch (MsgException e) {server->log << Name() << ": message exception: " << e.what() << std::endl; return;}

	//log << Name() << ": publishing on URI `" << report.uri() << "`" << std::endl;

	// PubSub the message!
	publish.publish(std::move(msg));
}
