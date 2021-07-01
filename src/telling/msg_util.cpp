#include <telling/msg_util.h>
#include <telling/msg_view.h>
#include <telling/msg_writer.h>


using namespace telling;


nng::msg ReplyableException::replyWithError(std::string_view error_context) const
{
	auto msg = WriteReply(replyStatus());
	msg.writeHeader("Content-Type", "text/plain");

	auto body = msg.writeBody();

	if (error_context.length())
	{
		body << " in `" << error_context << "`:\r\n\t";
	}
	body << what() << "\r\n";

	return msg.release();
}


nng::msg MsgException::replyWithError(std::string_view error_context) const
{
	auto msg = WriteReply(replyStatus());
	msg.writeHeader("Content-Type", "text/plain");

	auto body = msg.writeBody();

	if (error_context.length())
	{
		body << " in `" << error_context << "`:\r\n\t";
	}
	body << what();

	if (position && length)
	{
		body << "\r\nAt location:\r\n\t`"
			<< std::string_view(position, length);
	}
	body << "`\r\n";

	return msg.release();
}
