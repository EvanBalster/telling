#include <telling/msg_view.h>


using namespace telling;



void MsgView::_parse_msg()
{
	using namespace telling::detail;

	// Parse basic structure
	{
		nng::view view = msg.body().get();
		const char *begin = (char*) view.data(), *bol = begin, *pos = begin, *end = pos + view.size();

		// Start line
		auto startLine = ConsumeLine(pos, end);
		if (startLine.length() > 0xFFFF) throw MsgException(MsgError::START_LINE_MALFORMED, bol, pos-bol);
		_startLine_length = (uint16_t) startLine.length();
		if (pos == end) throw MsgException(MsgError::HEADER_INCOMPLETE, bol, pos-bol);

		// Headers
		_headers.start = (uint16_t) (pos - begin);
		auto boh = pos;
		while (true)
		{
			bol = pos;
			auto line = ConsumeLine(pos, end);
			if (line.length() == 0) break;
			if (pos == end) throw MsgException(MsgError::HEADER_INCOMPLETE, bol, pos-bol);
		}
		_headers.length = (uint16_t) (bol - boh);

		// Body
		if ((pos - begin) > 0xFFFF) throw MsgException(MsgError::HEADER_TOO_BIG, bol, pos-bol);
		_body_offset = (uint16_t) (pos - begin);
	}


	// ------------------------------------
	// ------    PARSE START-LINE    ------
	// ------------------------------------


	static const size_t MAX_PARTS = 4;
	std::string_view parts[MAX_PARTS];
	size_t count = 0;

	auto line = startLine();
	const char *beg = line.data(), *i=beg, *end = i+line.length();

	// Delimit the start-line into up to 4 parts.
	while (i < end)
	{
		if (count == MAX_PARTS-1)
		{
			parts[count] = std::string_view(i, end-i);
			break;
		}

		const char *word = i;
		while (i < end && *i != ' ') ++i;
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
	if (_type < TYPE(0))
	{
		int protocolPos = 3;
		while (protocolPos-- > 0)
		{
			if (MsgProtocol::Parse(parts[protocolPos])) break;
		}

		// Unknown protocol, cam't autodetect message type
		if (protocolPos < 0)
			throw MsgException(MsgError::UNKNOWN_PROTOCOL, line.data(), line.length());
		
		// Values of TYPE correspond to protocol position
		_type = TYPE(protocolPos);
	}
	

	auto toRange   = [beg]    (const std::string_view &s) {return HeadRange{(uint16_t) (s.data()-beg), (uint16_t) s.length()};};
	auto restRange = [beg,end](const std::string_view &s) {return HeadRange{(uint16_t) (s.data()-beg), (uint16_t) (end-s.data())};};

	switch (_type)
	{
	case TYPE::REPLY: // Reply
		if (count < 3) throw MsgException(MsgError::START_LINE_MALFORMED, line.data(), line.length());
		_protocol = toRange  (parts[0]);
		_status   = toRange  (parts[1]);
		_reason   = restRange(parts[2]);
		break;

	case TYPE::BULLETIN: // Bulletin
		if (count < 4) throw MsgException(MsgError::START_LINE_MALFORMED, line.data(), line.length());
		_uri      = toRange  (parts[0]);
		_protocol = toRange  (parts[1]);
		_status   = toRange  (parts[2]);
		_reason   = restRange(parts[3]);
		break;

	case TYPE::REQUEST: // Request
		if (count != 3) throw MsgException(MsgError::START_LINE_MALFORMED, line.data(), line.length());
		_method   = toRange  (parts[0]);
		_uri      = toRange  (parts[1]);
		_protocol = toRange  (parts[2]);
		break;
	}
}
