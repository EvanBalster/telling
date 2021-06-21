#include <telling/msg_writer.h>


using namespace telling;


/*
	TODO: optimize using a new NNG "grow" call
*/


static size_t NumDigits(size_t value)
{
	size_t digits = 0;
	do {++digits; value /= 10;} while (value);
	return digits;
}


static bool ContainsNewline(const std::string_view &s)
{
	for (auto c : s) if (c == '\r' || c == '\n') return true;
	return false;
}
static bool ContainsWhitespace(const std::string_view &s)
{
	for (auto c : s) if (c == '\r' || c == '\n' || c == ' ' || c == '\t') return true;
	return false;
}


MsgWriter::MsgWriter(MsgProtocol _protocol) :
	protocol(_protocol)
{
	crlf =
		protocol.code >= MsgProtocolCode::Http_1_0 &&
		protocol.code <= MsgProtocolCode::Http_1_1;
}


void MsgWriter::_startMsg()
{
	if (msg) throw MsgException(MsgError::OUT_OF_ORDER, 0, 0);
	*this = MsgWriter(protocol);
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
	msg.body().append(crlf ? nng::view("\r\n", 2) : nng::view("\n", 1));
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

	if (ContainsWhitespace(uri))
		throw MsgException(MsgError::START_LINE_MALFORMED, 0, 0);
	_append(uri);
	_append(' ');
	_append(protocol.toString());

	_newline();
}

void MsgWriter::startReply(Status status, std::string_view reason)
{
	_startMsg();

	_append(protocol.toString());
	_append(' ');
	_append(status.toString());
	_append(' ');

	if (ContainsNewline(reason))
		throw MsgException(MsgError::START_LINE_MALFORMED, 0, 0);
	_append(reason);

	_newline();
}

void MsgWriter::startBulletin(std::string_view uri, Status status, std::string_view reason)
{
	_startMsg();

	if (ContainsWhitespace(uri))
		throw MsgException(MsgError::START_LINE_MALFORMED, 0, 0);
	_append(uri);
	_append(' ');
	_append(protocol.toString());
	_append(' ');
	_append(status.toString());
	_append(' ');

	if (ContainsNewline(reason))
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
	if (ContainsNewline(value))
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

	if (lengthSize)
	{
		size_t bodySize = msg.body().size() - dataOffset;
		auto digits = NumDigits(bodySize);
		if (digits > lengthSize)
			throw nng::exception(nng::error::nospc, "Content-Length header completion");

		char *pos = msg.body().get().data<char>() + lengthOffset + digits;
		do
		{
			*--pos = '0' + (bodySize%10);
			bodySize /= 10;
		}
		while (bodySize);
	}
	

	return std::move(msg);
}


void MsgWriter::setNNGHeader(nng::view data)
{
	msg.header().clear();
	msg.header().append(data);
}



void MsgWriter::writeHeader_Allow(Methods methods)
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

	writeHeader("Allow", allowed);
}

void MsgWriter::writeHeader_Length(size_t maxLength)
{
	if (lengthSize)
		throw nng::exception(nng::error::nospc, "Content-Length header allocation");

	size_t digits = NumDigits(maxLength);

	_append("Content-Length:");

	lengthOffset = msg.body().size();
	lengthSize   = digits;

	_append(std::string_view("                    ", digits));
	_newline();
}
