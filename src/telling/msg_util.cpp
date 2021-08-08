#include <cctype>
#include <charconv>

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
		body << error_context << ": ";
	}
	body << what();

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

	if (excerpt.length())
	{
		body << "\r\nAt location:\r\n\t`"
			<< excerpt << "`";
	}
	body << "\r\n";

	return msg.release();
}


bool MsgHeaderView::is(std::string_view header_name) const noexcept
{
	if (name.size() != header_name.size()) return false;
	for (size_t i = 0; i < name.size(); ++i)
		if (std::tolower(name[i]) != std::tolower(header_name[i])) return false;
	return true;
}

int64_t MsgHeaderView::value_dec(int64_t int_value) const noexcept
{
	const char
		*b = &*value.begin(),
		*e = b + value.length();
	if (*b == '+') ++b;
	auto result = std::from_chars(b, e, int_value, 10);
	return int_value;
}