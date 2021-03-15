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
		
	methodString = consumeToSpace(i, e);
	uri          = consumeToSpace(i, e);
	protocol     = consumeToEOL  (i, e);

	method       = Method::Parse(methodString);
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

	protocol = consumeToSpace(i, e);

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

	uri      = consumeToSpace(i, e);
	protocol = consumeToSpace(i, e);

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
