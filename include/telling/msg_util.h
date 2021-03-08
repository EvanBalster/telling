#pragma once


#include <string_view>
#include <exception>

#include <nngpp/msg.h>


namespace telling
{
	class MsgError
	{
	public:
		enum ERROR
		{
			SUCCESS              = 0,
			HEADER_INCOMPLETE    = 1,
			HEADER_MALFORMED     = 2,
			START_LINE_MALFORMED = 3,
			OUT_OF_ORDER         = 4,
		};
	};

	using MSG_ERROR = MsgError::ERROR;


	class MsgException : public std::exception, public MsgError
	{
	public:
		MsgException(MSG_ERROR _error, const char *_position, size_t _length = 0) :
			error(_error), position(_position), length(_length) {}
		virtual ~MsgException() {}

		inline const char *what() const override
		{
			switch (error)
			{
			case SUCCESS:              return "The message was parsed successfully.";
			case HEADER_INCOMPLETE:    return "The message's header is incomplete.";
			case HEADER_MALFORMED:     return "The message contains a malformed header.";
			case START_LINE_MALFORMED: return "The message's start line is malformed.";
			default:                   return "An unknown error occurred while parsing the message.";
			}
		}

		/*
			Generate a reply message describing the error.
		*/
		nng::msg writeReply(std::string_view error_context) const;


	public:
		MSG_ERROR         error;
		const char *const position;
		size_t            length;
	};


	namespace detail
	{
		/*
			Support \n and \r\n newlines
		*/
		template<typename T, bool END_SAFE = true>
		inline std::basic_string_view<T> ConsumeLine(const T* &pos, const T *end)
		{
			const T *bol = pos, *eol;
			while (true)
			{
				if (END_SAFE) if (pos >= end) {eol = pos; break;}
				if (*pos == '\n') {eol = pos++; break;}
				if (*pos == '\r') {eol = pos++; if (pos != end && *pos == '\n') ++pos; break;}
				++pos;
			}
			return std::basic_string_view<T>(bol, eol-bol);
		}

		// Extract a line without end-of-data guard
		template<typename T>
		inline std::basic_string_view<T> ConsumeLine_unsafe(const T* &pos)    {ConsumeLine<T,false>(pos, nullptr);}


		// Matches ASCII whitespace characters ' ', \r, \f, \v, \n and \t
		inline bool IsWhitespace(uint32_t cp)    {--cp; return uint32_t(cp <= 31) & (uint32_t(0x80001F00) >> cp);}

		template<typename T, bool END_SAFE>
		inline void ConsumeWhitespace(const T* &pos, const T *end)
		{
			while (true)
			{
				if (END_SAFE) if (pos == end) break;
				if (IsWhitespace(*pos)) ++pos;
				else break;
			}
		}


		template<typename T, bool END_SAFE = true>
		inline std::basic_string_view<T> ConsumeWord(const T* &pos, const T *end)
		{
			ConsumeWhitespace<T,END_SAFE>(pos,end);

			const T *bow = pos;
			while (true)
			{
				if (END_SAFE) if (pos >= end) break;
				if (IsWhitespace(*pos)) break;
				else ++pos;
			}
			return std::basic_string_view<T>(bow, pos-bow);
		}

		// Extract a line without end-of-data guard
		template<typename T>
		inline std::basic_string_view<T> ExtractWord_unsafe(const T* &pos)    {ConsumeWord<T,false>(pos, nullptr);}
	}
}