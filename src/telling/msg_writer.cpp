#include <telling/msg_writer.h>


using namespace telling;


/*
	TODO: optimize using a new NNG "grow" call
*/


static uint8_t NumDigits(size_t value)
{
	uint8_t digits = 0;
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


/*
	Enable cout-style operations on msgbuf in this file only.
*/
static nng::msgbuf &operator<<(nng::msgbuf &o, std::string_view s)    {o.sputn(s.data(), s.length()); return o;}
static nng::msgbuf &operator<<(nng::msgbuf &o, char c)                {o.sputc(c); return o;}


MsgWriter::MsgWriter(MsgProtocol _protocol) :
	protocol(_protocol)
{
	crlf =
		protocol.code >= MsgProtocolCode::Http_1_0 &&
		protocol.code <= MsgProtocolCode::Http_1_1;
}


void MsgWriter::_startMsg()
{
	if (msg) throw MsgException(MsgError::ALREADY_WRITTEN, 0, 0);
	*this = MsgWriter(protocol);
	msg = nng::make_msg(0).release();
	out.open(msg, std::ios::out);
}

void MsgWriter::_autoCloseHeaders()
{
	if (!this->_p_body)
	{
		if (!msg) throw MsgException(MsgError::ALREADY_WRITTEN, 0, 0);

		// End headers
		_newline();
		out.pubsync();
		//this->_p_body = msg.body().size();

		// Parse everything  (TODO:  do this work along the way later)
		_parse_msg(msg.body().get());
	}
}

void MsgWriter::_newline()
{
	if (crlf) out.sputc('\r');
	out.sputc('\n');
}


void MsgWriter::startRequest(std::string_view uri, Method method)
{
	_startMsg();

	if (!method)
		throw MsgException(MsgError::START_LINE_MALFORMED, 0, 0);
	if (ContainsWhitespace(uri))
		throw MsgException(MsgError::START_LINE_MALFORMED, 0, 0);

	out << method.toString()
		<< ' ' << uri
		<< ' ' << protocol.toString();
	_newline();
}

void MsgWriter::startReply(Status status, std::string_view reason)
{
	_startMsg();

	if (ContainsNewline(reason))
		throw MsgException(MsgError::START_LINE_MALFORMED, 0, 0);

	out << protocol.toString()
		<< ' ' << status.toString()
		<< ' ' << reason;
	_newline();
}

void MsgWriter::startReport(std::string_view uri, Status status, std::string_view reason)
{
	_startMsg();

	if (ContainsWhitespace(uri))
		throw MsgException(MsgError::START_LINE_MALFORMED, 0, 0);
	if (ContainsNewline(reason))
		throw MsgException(MsgError::START_LINE_MALFORMED, 0, 0);

	out << uri
		<< ' ' << protocol.toString()
		<< ' ' << status.toString()
		<< ' ' << reason;
	_newline();
}

void MsgWriter::writeHeader(std::string_view name, std::string_view value)
{
	if (!msg || this->_p_body)
		throw MsgException(MsgError::ALREADY_WRITTEN, 0, 0);

	for (auto c : name) if (c == '\r' || c == '\n' || c == ':')
		throw MsgException(MsgError::HEADER_MALFORMED, 0, 0);
	if (ContainsNewline(value))
		throw MsgException(MsgError::HEADER_MALFORMED, 0, 0);

	out << name << ':' << value;
	_newline();
}


nng::msg MsgWriter::release()
{
	_autoCloseHeaders();
	out.close();

	if (head.lengthSize)
	{
		size_t bodySize = msg.body().size() - this->_p_body;
		auto digits = NumDigits(bodySize);
		if (digits > head.lengthSize)
			throw nng::exception(nng::error::nospc, "Content-Length header completion");

		char *pos = msg.body().get().data<char>() + head.lengthOffset + digits;
		do
		{
			*--pos = '0' + (bodySize%10);
			bodySize /= 10;
		}
		while (bodySize);
	}

	head = {};
	return std::move(Msg::release());
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
	if (head.lengthSize)
		throw nng::exception(nng::error::nospc, "Content-Length header allocation");

	uint8_t digits = NumDigits(maxLength);

	out << "Content-Length:";

	head.lengthOffset = (uint16_t) msg.body().size();
	head.lengthSize   = digits;

	// Unlikely we'll need to deal with messages >= 100 exabytes
	out << std::string_view("                    ", digits);
	_newline();
}
