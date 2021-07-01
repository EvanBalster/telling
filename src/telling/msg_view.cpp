#include <telling/msg_view.h>


using namespace telling;



void MsgLayout::_parse_msg(nng::view msg, TYPE _type)
{
	using namespace telling::detail;

	std::string_view startLine;
	_parse_reset();
	const char *header_beg = nullptr;

	// Parse basic structure
	{
		const char *begin = (char*) msg.data(), *bol = begin, *pos = begin, *end = pos + msg.size();

		// Start line
		startLine = ConsumeLine(pos, end);
		if (startLine.length() > 0xFFFF) throw MsgException(MsgError::START_LINE_MALFORMED, bol, pos-bol);
		if (pos == end) throw MsgException(MsgError::HEADER_INCOMPLETE, bol, pos-bol);

		// Headers
		header_beg = pos;
		auto boh = pos;
		while (true)
		{
			bol = pos;
			auto line = ConsumeLine(pos, end);
			if (line.length() == 0) break;
			if (pos == end) throw MsgException(MsgError::HEADER_INCOMPLETE, bol, pos-bol);
		}

		// Body
		if ((pos - begin) > 0xFFFF) throw MsgException(MsgError::HEADER_TOO_BIG, bol, pos-bol);
		_p_body = (uint16_t) (pos - begin);
	}


	// ------------------------------------
	// ------    PARSE START-LINE    ------
	// ------------------------------------


	static const size_t MAX_PARTS = 4;
	std::string_view parts[MAX_PARTS];
	size_t count = 0;

	const char *beg = startLine.data(), *i=beg, *end = i+startLine.length();

	// Delimit the start-line into up to 4 parts.
	while (i < end)
	{
		if (count == MAX_PARTS-1)
		{
			parts[count++] = std::string_view(i, end-i);
			break;
		}

		const char *word = i;
		while (i < end && *i != ' ') ++i;
		parts[count] = std::string_view(word, i-word);
		++i;
		++count;
	}

	/*
		Case    | word 1   | word 2   | word 3   | word 4
		--------|----------|----------|-------------------------
		REPLY   | PROTOCOL | STATUS   [ REASON-PHRASE ]
		REPORT  | URI      [ PROTOCOL | STATUS   | REASON-PHRASE ]
		REQUEST | METHOD   | URI      | PROTOCOL

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
			throw MsgException(MsgError::UNKNOWN_PROTOCOL, startLine.data(), startLine.length());
		
		// Values of TYPE correspond to protocol position
		_type = TYPE(protocolPos);
	}
	

	auto checkSize = [beg,end](size_t s, size_t lim)
	{
		if (s>lim) throw MsgException(MsgError::HEADER_TOO_BIG, beg, end-beg);
		return s;
	};
	auto len8_sp   = [&checkSize]           (const std::string_view &s) {return (uint8_t)  checkSize(s.length()+1, 255);};
	auto lenURI_sp = [&checkSize]           (const std::string_view &s) {return (uint16_t) checkSize(s.length()+1, 65535);};
	auto lenRea_nl = [&checkSize,header_beg](const std::string_view &s) {return (uint8_t)  checkSize(header_beg-s.data(), 255);};

	switch (_type)
	{
	case TYPE::REPLY:
		if (count < 2)
			throw MsgException(MsgError::START_LINE_MALFORMED, startLine.data(), startLine.length());
		_prt_sp = len8_sp  (parts[0]);
		_sts_sp = len8_sp  (parts[1]);
		_rea_nl = lenRea_nl(parts[2]);
		break;

	case TYPE::REPORT:
		if (count < 1)
			throw MsgException(MsgError::START_LINE_MALFORMED, startLine.data(), startLine.length());
		_uri_sp = lenURI_sp(parts[0]);
		_prt_sp = len8_sp  (parts[1]);
		_sts_sp = len8_sp  (parts[2]);
		_rea_nl = lenRea_nl(parts[3]);
		break;

	case TYPE::REQUEST:
		if (count < 2 || count > 3)
			throw MsgException(MsgError::START_LINE_MALFORMED, startLine.data(), startLine.length());
		_mth_sp = len8_sp  (parts[0]);
		_uri_sp = lenURI_sp(parts[1]);
		_prt_sp = len8_sp  (parts[2]);
		break;
	}
}
