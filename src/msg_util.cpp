#include <telling/msg_util.h>
#include <telling/msg_view.h>
#include <telling/msg_writer.h>


using namespace telling;


nng::msg MsgException::writeReply(std::string_view error_context) const
{
	auto msg = MsgWriter::Reply(StatusCode::BadRequest);
	msg.writeHeader("Content-Type", "text/plain");

	msg.writeData("Message Exception in `");
	msg.writeData(error_context);
	msg.writeData("`:\r\n\t`");
	msg.writeData(what());
	if (position && length)
	{
		msg.writeData("`\r\nAt location:\r\n\t`");
		msg.writeData(std::string_view(position, length));
	}
	msg.writeData("`\r\n");

	return msg.release();
}