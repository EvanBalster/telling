#include <telling/msg_view.h>


using namespace telling;


void MsgLayout::_setStartLine(
	std::string_view  lineWithEol,
	TYPE              type,
	const char       *second_elem,
	const char       *third_elem,
	const char       *fourth_elem)
{
	auto line = lineWithEol;
	if (line.length() < 2 || line.back() != '\n')
		throw MsgException(MsgError::START_LINE_MALFORMED, lineWithEol);
	line.remove_suffix((line[line.length()-2]=='\r') ? 2 : 1);

	auto checkSize = [line](size_t s, size_t lim)
	{
		if (s>lim) throw MsgException(MsgError::HEADER_TOO_BIG, line);
		return s;
	};


	// Line parameters
	_sl_len = (uint16_t) checkSize(line.length(), 65535);
	_sl_nl  = (uint8_t) (lineWithEol.length() - line.length());

	const char
		*BOL = line.data(),
		*EOL = BOL+line.length();

	const char *reason = nullptr;

	static const size_t
		MAX_URI_POS  = MAX_METHOD_LENGTH + 1,
		MAX_STS_RPOS = MAX_REASON_LENGTH + MAX_STATUS_LENGTH + 1,
		MAX_PRT_RPOS = MAX_STS_RPOS + MAX_PROTOCOL_LENGTH + 1;


	switch (type)
	{
	default:
		// Invalid type
		goto start_line_malformed;
		break;

	case TYPE::REQUEST:
		// URI required, protocol optional; no fourth element.
		if (!second_elem || fourth_elem) goto start_line_malformed;

		_uri_pos                  = (uint8_t) checkSize(second_elem - BOL, MAX_URI_POS);
		if (third_elem) _prt_rpos = (uint8_t) checkSize(EOL - third_elem, MAX_PRT_RPOS);
		else            _prt_rpos = 0;
		_sts_rpos = 0;

		// Don't verify reason/status
		return;

	case TYPE::REPLY:
		// URI required; reason-phrase optional; no fourth element
		if (!second_elem) goto start_line_malformed;

		_uri_pos = 0;
		_prt_rpos = (uint8_t) checkSize(line.length(), MAX_PRT_RPOS);
		_sts_rpos = (uint8_t) checkSize(EOL - second_elem, MAX_STS_RPOS);
		reason = third_elem; // and fourth, etc.
		
		//_prt_sp = len8_sp  (parts[0]);
		//_sts_sp = len8_sp  (parts[1]);
		//_rea_nl = ((part_count > 2) ? lenRea_nl(parts[2]) : len_nl());
		break;

	case TYPE::REPORT:
		// URI required; all other elements optional

		_uri_pos = 0;
		if (!second_elem)
		{
			_prt_rpos = 0;
			_sts_rpos = 0;
			if (third_elem || fourth_elem) goto start_line_malformed;
			break;
		}
		_prt_rpos = (uint8_t) checkSize(EOL - second_elem, MAX_PRT_RPOS);
		if (!third_elem)
		{
			_sts_rpos = 0;
			if (fourth_elem) goto start_line_malformed;
			break;
		}
		_sts_rpos = (uint8_t) checkSize(EOL - third_elem, MAX_STS_RPOS);
		reason = fourth_elem;

		break;
	}

	// Verify reason/status
	if (_sts_rpos)
	{
		if (reason)    {if (_sts_rpos < 4) goto start_line_malformed;}
		else           {if (_sts_rpos != 3) goto start_line_malformed;}
	}

	return;

start_line_malformed:
	throw MsgException(MsgError::START_LINE_MALFORMED, line);
}


void MsgLayout::_parse_msg(nng::view msg, TYPE _type)
{
	using namespace telling::detail;
	
	if (msg.size() == 0)
		throw MsgException(MsgError::HEADER_INCOMPLETE, "Message data is empty (no header)");
	if (!msg.data())
		throw MsgException(MsgError::HEADER_INCOMPLETE, "Message data pointer is null");

	std::string_view startLine;
	_parse_reset();
	const char *header_beg = nullptr;

	// Parse basic structure
	{
		const char *begin = (char*) msg.data(), *bol = begin, *pos = begin, *end = pos + msg.size();

		// Start line
		startLine = ConsumeLine(pos, end);
		if (startLine.length() > 0xFFFF) throw MsgException(MsgError::HEADER_TOO_BIG, "Start line >= 64 KiB");
		if (pos == end) throw MsgException(MsgError::HEADER_INCOMPLETE, startLine);

		// Headers
		header_beg = pos;
		auto boh = pos;
		while (true)
		{
			bol = pos;
			auto line = ConsumeLine(pos, end);
			if (line.length() == 0) break;
			if (pos == end) throw MsgException(MsgError::HEADER_INCOMPLETE, line);
		}

		// Body
		if ((pos - begin) > 0xFFFF) throw MsgException(MsgError::HEADER_TOO_BIG,
			"Headers >= 64 KiB; missing empty line?");
		_p_body = (uint16_t) (pos - begin);
	}


	// ------------------------------------
	// ------    PARSE START-LINE    ------
	// ------------------------------------


	static const size_t MAX_PARTS = 4;
	std::string_view parts[MAX_PARTS];
	size_t part_count = 0;

	const char *beg = startLine.data(), *i=beg, *end = i+startLine.length();

	// Delimit the start-line into up to 4 parts.
	while (i < end)
	{
		if (part_count == MAX_PARTS-1)
		{
			parts[part_count++] = std::string_view(i, end-i);
			break;
		}

		const char *word = i;
		while (i < end && *i != ' ') ++i;
		parts[part_count] = std::string_view(word, i-word);
		++i;
		++part_count;
	}

	// Respect empty final element if present
	if (i > beg && *(i-1) == ' ' && part_count < MAX_PARTS)
		parts[part_count++] = std::string_view(i, 0);

	/*
		Case    | word 1   | word 2   | word 3   | word 4
		--------|----------|----------|-------------------------
		REPLY   | PROTOCOL | STATUS   [ REASON-PHRASE ]
		REPORT  | URI      [ PROTOCOL | STATUS   | REASON-PHRASE ]
		REQUEST | METHOD   | URI      | PROTOCOL

		...URI can be anything, and REASON-PHRASE may contain spaces.
		So our strategy is to search backwards for a matching protocol.
	*/
	if (_type < TYPE(0)) switch (part_count)
	{
	case 0: break;
	case 1:
		_type = TYPE::REPORT; // Single-element start line must be a report (URI).
		break;
	default:
		// If the third element is a valid protocol, this must be a request.
		if (MsgProtocol::Parse(parts[2])) {_type = TYPE::REQUEST; break;}

	case 2:
		// If the second element is a valid protocol, this must be a report.
		//  Degenerate case: "GET HTTP/1.1"
		if (MsgProtocol::Parse(parts[1])) {_type = TYPE::REPORT; break;}

		// A reply will start with a protocol,
		//   Which will tend to be blank or contain a slash.
		if (parts[0].length() == 0 ||
			parts[0].find_first_of('/') != std::string::npos)
			{_type = TYPE::REPLY; break;}

		// Assume the slash-less first element is a method, even if unrecognized.
		_type = TYPE::REQUEST;
		break;
	}
	

	std::string_view startLineWithNL(beg, header_beg-beg);

	_setStartLine(startLineWithNL, _type, parts[1].data(), parts[2].data(), parts[3].data());
}
