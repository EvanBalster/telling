#include <nngpp/protocol/sub0.h>
#include <telling/server.h>


using namespace telling;



Server::Publish::Publish() :
	subscribe(sub_delegate = std::make_shared<Delegate_Sub>(this))
{
	auto server = this->server();
	auto &log = server->log;

	// The subscriber is a relay, and accepts all topics.
	subscribe.subscribe("");

	// Relay service events to internal modules (dial-in mvechanism)
	subscribe.listen(server->address_internal);
}
Server::Publish::~Publish()
{
	sub_delegate->stop();

	subscribe.disconnectAll();
	publish.disconnectAll();
}


Directive Server::Publish::receive_error(Delegate_Sub  *, nng::error error)
{
	server()->log << Name() << ": ingestion error: " << nng::to_string(error) << std::endl;
	return Directive::AUTO;
}

Directive Server::Publish::received(const MsgView::Bulletin &bulletin, nng::msg &&msg)
{
	// No mutex needed; this AIO is the only sender.

	auto server = this->server();
	auto &log = server->log;

	log << Name() << ": publishing on URI `" << bulletin.uri() << "`" << std::endl;

	// Publish the message!
	publish.publish(std::move(msg));

	return Directive::CONTINUE;
}
