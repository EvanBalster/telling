#include <telling/msg_view.h>


using namespace telling;



void MsgView::_parse_msg()
{
	using namespace telling::detail;

	nng::view view = msg.body().get();
	const char *begin = (char*) view.data(), *bol = begin, *pos = begin, *end = pos + view.size();

	// Start line
	auto startLine = ConsumeLine(pos, end);
	startLine_length = startLine.length();
	if (pos == end) throw MsgException(MsgError::HEADER_INCOMPLETE, bol, pos-bol);

	// Headers
	msgHeaders.string = std::string_view(pos, 0);
	while (true)
	{
		bol = pos;
		auto line = ConsumeLine(pos, end);
		if (line.length() == 0) break;
		if (pos == end) throw MsgException(MsgError::HEADER_INCOMPLETE, bol, pos-bol);
	}
	msgHeaders.string = std::string_view(msgHeaders.string.data(), bol - msgHeaders.string.data());

	// Body
	body_offset = pos - begin;
}


void MsgView::_parse_auto()
{
	static const size_t MAX_PARTS = 4;
	std::string_view parts[MAX_PARTS];
	size_t count = 0;

	auto line = startLine();
	const char *beg = line.data(), *i=beg, *e = i+line.length();

	// Delimit the start-line into up to 4 parts.
	while (i < e)
	{
		if (count == MAX_PARTS-1)
		{
			parts[count] = std::string_view(i, e-i);
		}

		const char *word = i;
		while (i < e && *i != ' ') ++i;
		parts[count] = std::string_view(word, i-word);
		++i;
		++count;
	}

	/*
		Case     | word 1   | word 2   | word 3   | word 4
		---------|----------|----------|-------------------------
		REPLY    | PROTOCOL | STATUS   | REASON-PHRASE
		BULLETIN | URI      | PROTOCOL | STATUS   | REASON-PHRASE
		REQUEST  | METHOD   | URI      | PROTOCOL |

		...URI can be anything, and REASON-PHRASE may contain spaces.
		So our strategy is to search backwards for a matching protocol.
	*/
	size_t protocolPos = 3;
	while (protocolPos--)
	{
		protocolString = parts[protocolPos];
		protocol       = MsgProtocol::Parse(protocolString);
		if (protocol) break;
	}

	switch (protocolPos)
	{
	default:
		// Unknown protocol
		throw MsgException(MsgError::HEADER_MALFORMED, line.data(), line.length());

	case 0: // Reply
		if (count < 3) throw MsgException(MsgError::HEADER_MALFORMED, line.data(), line.length());
		statusString = parts[1];
		reason       = std::string_view(parts[2].data(), line.end()-parts[2].begin());
		status       = Status::Parse(statusString);
		break;

	case 1: // Bulletin
		if (count < 4) throw MsgException(MsgError::HEADER_MALFORMED, line.data(), line.length());
		uri          = parts[0];
		statusString = parts[2];
		reason       = std::string_view(parts[3].data(), line.end()-parts[3].begin());
		status       = Status::Parse(statusString);
		break;

	case 2: // Request
		if (count != 3) throw MsgException(MsgError::HEADER_MALFORMED, line.data(), line.length());
		methodString   = parts[0];
		uri            = parts[1];
		method         = Method::Parse(methodString);
		break;
	}
}


void MsgView::Request::_parse_request()
{
	auto line = startLine();

	const char *i = line.data(), *e = i+line.length();

	const char *bol = i;

	auto consumeToSpace = [bol](const char *&i, const char *e)
	{
		const char *bow = i;
		while (true)
		{
			if (i == e) throw MsgException(MsgError::HEADER_MALFORMED, bol, e-bol);
			if (*i == ' ') return std::string_view(bow, (i++)-bow);
			++i;
		}
	};
	auto consumeToEOL = [bol](const char *&i, const char *e)
	{
		const char *bow = i;
		while (true)
		{
			if (i == e || *i == '\r' || *i == '\n') return std::string_view(bow, i-bow);
			if (*i == ' ') throw MsgException(MsgError::HEADER_MALFORMED, bol, e-bol);
			++i;
		}
	};
		
	methodString   = consumeToSpace(i, e);
	uri            = consumeToSpace(i, e);
	protocolString = consumeToEOL  (i, e);

	method         =      Method::Parse(methodString);
	protocol       = MsgProtocol::Parse(protocolString);
}


void MsgView::Reply::_parse_reply()
{
	auto line = startLine();

	const char *i = line.data(), *e = i+line.length();

	const char *bol = i;

	auto consumeToSpace = [bol](const char *&i, const char *e)
	{
		const char *bow = i;
		while (true)
		{
			if (i == e) throw MsgException(MsgError::START_LINE_MALFORMED, bol, e-bol);
			if (*i == ' ') return std::string_view(bow, (i++)-bow);
			++i;
		}
	};
	auto consumeToEOL = [bol](const char *&i, const char *e)
	{
		const char *bow = i;
		while (true)
		{
			// Spaces allowed in reason phrase
			if (i == e || *i == '\r' || *i == '\n') return std::string_view(bow, i-bow);
			++i;
		}
	};

	protocolString = consumeToSpace(i, e);
	protocol       = MsgProtocol::Parse(protocolString);

	// 3-digit status code...
	if (i + 4 > e) throw MsgException(MsgError::START_LINE_MALFORMED, bol, e-bol);
	const char *statusStart = i;
	for (size_t n = 0; n < 3; ++n)
	{
		if (*i < '0' || *i > '9')
			throw MsgException(MsgError::START_LINE_MALFORMED, bol, e-bol);
		++i;
	}
	statusString = std::string_view(statusStart, 3);

	// Space after status code is required
	if (*i != ' ') throw MsgException(MsgError::START_LINE_MALFORMED, bol, e-bol);
	++i;

	// Reason phrase, which may be 0 characters
	reason = consumeToEOL(i, e);
}


void MsgView::Bulletin::_parse_bulletin()
{
	auto line = startLine();

	const char *i = line.data(), *e = i+line.length();

	const char *bol = i;

	auto consumeToSpace = [bol](const char *&i, const char *e)
	{
		const char *bow = i;
		while (true)
		{
			if (i == e) throw MsgException(MsgError::START_LINE_MALFORMED, bol, e-bol);
			if (*i == ' ') return std::string_view(bow, (i++)-bow);
			++i;
		}
	};
	auto consumeToEOL = [bol](const char *&i, const char *e)
	{
		const char *bow = i;
		while (true)
		{
			// Spaces allowed in reason phrase
			if (i == e || *i == '\r' || *i == '\n') return std::string_view(bow, i-bow);
			++i;
		}
	};

	uri            = consumeToSpace(i, e);
	protocolString = consumeToSpace(i, e);
	protocol       = MsgProtocol::Parse(protocolString);

	// 3-digit status code...
	if (i + 4 > e) throw MsgException(MsgError::START_LINE_MALFORMED, bol, e-bol);
	const char *statusStart = i;
	for (size_t n = 0; n < 3; ++n)
	{
		if (*i < '0' || *i > '9')
			throw MsgException(MsgError::START_LINE_MALFORMED, bol, e-bol);
		++i;
	}
	statusString = std::string_view(statusStart, 3);

	// Space after status code is required
	if (*i != ' ') throw MsgException(MsgError::START_LINE_MALFORMED, bol, e-bol);
	++i;

	// Reason phrase, which may be 0 characters
	reason = consumeToEOL(i, e);
}
