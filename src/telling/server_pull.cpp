#include <telling/server.h>


using namespace telling;



Server::Pull::Pull() :
	pull(pull_delegate = std::make_shared<Delegate_Pull>(this))
{
	auto server = this->server();
	auto &log = server->log;
}
Server::Pull::~Pull()
{
	pull_delegate->stop();
	pull.disconnectAll();
}


AsyncOp::Directive Server::Pull::receive_error(Delegate_Pull  *, nng::error error)
{
	server()->log << Name() << ": ingestion error: " << nng::to_string(error) << std::endl;
	return AsyncOp::AUTO;
}

AsyncOp::Directive Server::Pull::received(const MsgView::Request &request, nng::msg &&msg)
{
	auto server = this->server();

	auto status = server->services.routePush(request.uri(), std::move(msg));

	//server->log << Name() << ": pushing to URI `" << request.uri << "`" << std::endl;

	if (status.isSuccessful())
	{
		return AsyncOp::CONTINUE;
	}
	else
	{
		// Log the error.
		server->log << Name() << ": error " << status << " (" << status.reasonPhrase()
			<< ") routing to `" << request.uri() << "`" << std::endl;

		// There's no opportunity to reply, so the failed message is discarded.
		return AsyncOp::DECLINE;
	}
}
