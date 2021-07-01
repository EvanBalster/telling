#include <telling/service_reactor.h>
#include <telling/msg_writer.h>


using namespace telling;


Reactor::Reactor(UriView uri_prefix) :
	_uri_prefix(uri_prefix)
{
}

void Reactor::_handle(Query query, nng::msg &&_msg)
{
	try
	{
		Msg::Request request = Msg::Request(std::move(_msg));
		Method       method = request.method();

		// Prefix check
		if (!request.uri().hasPrefix(_uri_prefix))
			throw status_exceptions::BadGateway(_uri_prefix);


		std::lock_guard g(_reactor_mutex);

		nng::msg rep;

		switch (method.code)
		{
		default:
		case MethodCode::CONNECT: break;
		case MethodCode::None:    break;

			// Safe
		case MethodCode::GET:     this->async_get    (query, std::move(request)); break;
		case MethodCode::HEAD:    this->async_head   (query, std::move(request)); break;
		case MethodCode::OPTIONS: this->async_options(query, std::move(request)); break;
		case MethodCode::TRACE:   this->async_trace  (query, std::move(request)); break;

			// Idempotent
		case MethodCode::PUT:     this->async_put    (query, std::move(request)); break;
		case MethodCode::DELETE:  this->async_delete (query, std::move(request)); break;

			// Other
		case MethodCode::PATCH:   this->async_patch  (query, std::move(request)); break;
		case MethodCode::POST:    this->async_post   (query, std::move(request)); break;

			// Nonstandard
		case MethodCode::Unknown: this->async_UNKNOWN(query, std::move(request)); break;
		}
	}
	catch (ReplyableException &ex)
	{
		if (query.reply)
		{
			query.reply(ex.replyWithError(_uri_prefix));
		}

		// TODO logging?
	}
	catch (nng::exception &ex)
	{
		if (query.reply)
		{
			auto msg = WriteReply(StatusCode::InternalServerError);
			msg.writeHeader("Content-Type", "text/plain");

			msg.writeBody()
				<< "NNG exception in `" << _uri_prefix << "`:\r\n\t"
				<< ex.what() << " -- " << ex.who();

			query.reply(msg.release());
		}

		// TODO logging?
	}
	catch (std::exception &ex)
	{
		if (query.reply)
		{
			auto msg = WriteReply(StatusCode::InternalServerError);
			msg.writeHeader("Content-Type", "text/plain");

			msg.writeBody()
				<< "C++ exception in `" << _uri_prefix << "`:\r\n\t"
				<< ex.what();

			query.reply(msg.release());
		}

		// TODO logging?
	}
}


void Reactor::async_trace(Query query, Msg::Request &&req)
{
	auto reply = WriteReply(StatusCode::OK);
	reply.writeHeader("Content-Type", "http");
	return query.reply(reply.release());
}

void Reactor::async_options(Query query, Msg::Request &&req)
{
	auto reply = WriteReply();
	reply.writeHeader_Allow(this->allowed(req.uri()));
	return query.reply(reply.release());
}

nng::msg Reactor::NotImplemented(UriView uri)
{
	auto reply = WriteReply(StatusCode::NotImplemented);
	reply.writeHeader_Allow(this->allowed(uri));
	return reply.release();
}
