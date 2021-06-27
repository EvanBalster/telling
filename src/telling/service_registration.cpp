#include <telling/service.h>
#include <telling/msg_view.h>
#include <telling/msg_writer.h>


using namespace telling;


class Registration::Delegate : public AsyncRequest
{
public:
	STATUS         status = INITIAL;
	nng::exception except  = nng::exception(nng::error::success);

	void async_prep (Requesting, nng::msg &query) final
	{
	}
	void async_sent (Requesting)             final
	{
		status = REQUESTED;
	}
	void async_recv(Requesting, nng::msg &&response) final
	{
		auto reply = MsgView::Reply(response);

		auto repStatus = reply.status();

		if (repStatus.isSuccessful())
		{
			status = ENLISTED;
		}
		else
		{
			const char *errSrc = "Registration Reply Status";
			nng::error errType = nng::error::internal;

			if      (repStatus.isClientError())
			{
				errSrc = "Registration Request Error";
				switch (repStatus.code)
				{
				case HttpStatus::Code::Unauthorized: errType = nng::error::perm;      break;
				case HttpStatus::Code::Conflict:     errType = nng::error::addrinuse; break;
				case HttpStatus::Code::NotFound:     errType = nng::error::exist;     break;
				case HttpStatus::Code::URITooLong:   errType = nng::error::addrinval; break;
				default:                                                              break;
				}
			}
			else if (repStatus.isServerError())
			{
				errSrc = "Registration Server Error";
			}
			else if (repStatus.isInformational())
			{
				errSrc = "Registration Informational Reply";
			}
			else if (repStatus.isRedirection())
			{
				errSrc = "Registration Redirection";
			}

			status = FAILED;
			except = nng::exception(errType, errSrc);
		}
	}
	void async_error(Requesting req, AsyncError error) final
	{
		status = FAILED;
		except = nng::exception(error, "Registration Networking Error");
	}
};



Registration::Registration(
	std::string_view servicePath,
	std::string_view servicePath_alias,
	std::string_view serverID) :
	delegate(std::make_shared<Delegate>()),
	requester(delegate)
{
	requester.dial(HostAddress::Base::InProc(std::string(serverID) + "/register"));

	if (!servicePath_alias.length()) servicePath_alias = servicePath;

	MsgWriter msg = WriteRequest("*services", MethodCode::POST);
	msg.writeData(servicePath_alias);
	msg.writeData("\n");
	msg.writeData(servicePath);

	requester.request(msg.release());
}

Registration::~Registration()
{

}

Registration::STATUS Registration::status() const noexcept
{
	return delegate->status;
}

const nng::exception &Registration::exception() const noexcept
{
	return delegate->except;
}