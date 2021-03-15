#include <telling/service.h>
#include <telling/msg_view.h>
#include <telling/msg_writer.h>


using namespace telling;


class Registration::Delegate : public AsyncQuery
{
public:
	STATUS         status = INITIAL;
	nng::exception except  = nng::exception(nng::error::success);

	Directive asyncQuery_made (QueryID, const nng::msg &query) final
	{
		return CONTINUE;
	}
	Directive asyncQuery_sent (QueryID)                        final
	{
		status = REQUESTED;
		return CONTINUE;
	}
	Directive asyncQuery_done (QueryID, nng::msg &&response)   final
	{
		auto reply = MsgView::Reply(response);

		auto repStatus = reply.status();

		if (repStatus.isSuccessful())
		{
			status = ENLISTED;
			return CONTINUE;
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
			return TERMINATE;
		}
	}
	Directive asyncQuery_error(QueryID, nng::error error)      final
	{
		status = FAILED;
		except = nng::exception(error, "Registration Networking Error");
		return CONTINUE;
	}
};



Registration::Registration(
	std::string_view serverID,
	std::string_view serviceURI,
	std::string_view serviceURI_register_as) :
	delegate(std::make_shared<Delegate>()),
	requester(delegate)
{
	requester.dial(HostAddress::Base::InProc(serverID));

	MsgWriter msg = MsgWriter::Request("*services", MethodCode::POST);
	msg.writeData(serviceURI_register_as);
	msg.writeData("\n");
	msg.writeData(serviceURI);

	requester.request(msg.release());
}
Registration::Registration(
	std::string_view serverID,
	std::string_view serviceURI) :
	Registration(serverID, serviceURI, serviceURI)
{
}

Registration::~Registration()
{

}

Registration::STATUS Registration::status() const noexcept
{
	return static_cast<Delegate*>(&*delegate)->status;
}

const nng::exception &Registration::exception() const noexcept
{
	return static_cast<Delegate*>(&*delegate)->except;
}