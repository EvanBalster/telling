#include <telling/msg_writer.h>


using namespace telling;


/*
	TODO: optimize using a new NNG "grow" call
*/



void MsgWriter::_startMsg()
{
	if (msg) throw MsgException(MsgError::OUT_OF_ORDER, 0, 0);
	*this = MsgWriter();
	msg = nng::make_msg(0);
}

void MsgWriter::_autoCloseHeaders()
{
	if (!msg) throw MsgException(MsgError::OUT_OF_ORDER, 0, 0);
	if (!dataOffset)
	{
		// End headers
		_newline();
		dataOffset = msg.body().size();
	}
}

void MsgWriter::_newline()
{
	msg.body().append(nng::view("\n", 1));
}

void MsgWriter::_append(char c)
{
	msg.body().append(nng::view(&c, 1));
}
void MsgWriter::_append(const std::string_view &s)
{
	msg.body().append(nng::view(s.data(), s.length()));
}


void MsgWriter::startRequest(std::string_view uri, Method method)
{
	_startMsg();

	if (!method)
		throw MsgException(MsgError::START_LINE_MALFORMED, 0, 0);

	_append(method.toString());
	_append(' ');

	for (auto c : uri) if (c == '\r' || c == '\n' || c == ' ')
		throw MsgException(MsgError::START_LINE_MALFORMED, 0, 0);
	_append(uri);
	_append(' ');

	// PROTOCOL?

	_newline();
}

void MsgWriter::startReply(Status status, std::string_view reason)
{
	_startMsg();

	// PROTOCOL?

	_append(' ');
	_append(status.toString());
	_append(' ');

	for (auto c : reason) if (c == '\r' || c == '\n')
		throw MsgException(MsgError::START_LINE_MALFORMED, 0, 0);
	_append(reason);

	_newline();
}

void MsgWriter::startBulletin(std::string_view uri, Status status, std::string_view reason)
{
	_startMsg();

	for (auto c : uri) if (c == '\r' || c == '\n' || c == ' ')
		throw MsgException(MsgError::START_LINE_MALFORMED, 0, 0);
	_append(uri);
	_append(' ');

	// PROTOCOL?

	_append(' ');
	_append(status.toString());
	_append(' ');

	for (auto c : reason) if (c == '\r' || c == '\n')
		throw MsgException(MsgError::START_LINE_MALFORMED, 0, 0);
	_append(reason);

	_newline();
}

void MsgWriter::writeHeader(std::string_view name, std::string_view value)
{
	if (!msg || dataOffset)
		throw MsgException(MsgError::OUT_OF_ORDER, 0, 0);

	for (auto c : name) if (c == '\r' || c == '\n' || c == ':')
		throw MsgException(MsgError::HEADER_MALFORMED, 0, 0);
	for (auto c : value) if (c == '\r' || c == '\n')
		throw MsgException(MsgError::HEADER_MALFORMED, 0, 0);

	_append(name);
	_append(':');
	_append(value);
	_newline();
}


void MsgWriter::writeData(std::string_view text)
{
	_autoCloseHeaders();
	_append(text);
}

void MsgWriter::writeData(nng::view data)
{
	writeData(std::string_view((const char*) data.data(), data.size()));
}


nng::msg &&MsgWriter::release()
{
	_autoCloseHeaders();
	return std::move(msg);
}


void MsgWriter::setNNGHeader(nng::view data)
{
	msg.header().clear();
	msg.header().append(data);
}



void MsgWriter::writeHeader_Allowed(Methods methods)
{
	std::string allowed;

	MethodCode m = MethodCode::None;

	bool first = true;
	while (true)
	{
		m = MethodCode(unsigned(m)+1);
		if (m >= MethodCode::EndOfValidMethods) break;

		if (methods.contains(m))
		{
			if (first) first = false;
			else       allowed.append(", ");
			allowed.append(Method(m).toString());
		}
	}

	writeHeader("Allowed", allowed);
}
