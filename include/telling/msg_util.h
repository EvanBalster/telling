#pragma once


#include <string_view>
#include <exception>

#include <nngpp/msg.h>

#include "msg_status.h"


namespace telling
{
	/*
		An interface for exceptions that can be written as replies.
	*/
	class ReplyableException : public std::exception
	{
	public:
		virtual Status   replyStatus()                                 const noexcept = 0;
		virtual nng::msg replyWithError(std::string_view context = "") const;
	};


	/*
		Namespace
	*/
	namespace status_exceptions
	{
		template<StatusCode CODE>
		class Err_
		{
		public:
			const std::string message;

		public:
			Err_()                        : message(Status(CODE).toString()) {}
			Err_(std::string _message)    : message(_message) {}

			Status replyStatus() const noexcept override     {return CODE;}

			inline const char *what() const override         {return message.c_str();}
		};

		// 400 series
		using BadRequest       = Err_<StatusCode::BadRequest>;
		using Unauthorized     = Err_<StatusCode::Unauthorized>;

		using Forbidden        = Err_<StatusCode::Forbidden>;
		using NotFound         = Err_<StatusCode::NotFound>;
		using MethodNotAllowed = Err_<StatusCode::MethodNotAllowed>;
		using NotAcceptable    = Err_<StatusCode::NotAcceptable>;
		using RequestTimeout   = Err_<StatusCode::RequestTimeout>;
		using Conflict         = Err_<StatusCode::Conflict>;
		using Gone             = Err_<StatusCode::Gone>;
		using LengthRequired   = Err_<StatusCode::LengthRequired>;

		using PayloadTooLarge             = Err_<StatusCode::PayloadTooLarge>;
		using URITooLong                  = Err_<StatusCode::URITooLong>;
		using UnsupportedMediaType        = Err_<StatusCode::UnsupportedMediaType>;
		using RangeNotSatisfiable         = Err_<StatusCode::RangeNotSatisfiable>;

		using UnprocessableEntity         = Err_<StatusCode::UnprocessableEntity>;
		using Locked                      = Err_<StatusCode::Locked>;
		using FailedDependency            = Err_<StatusCode::FailedDependency>;
		using TooManyRequests             = Err_<StatusCode::TooManyRequests>;
		using RequestHeaderFieldsTooLarge = Err_<StatusCode::RequestHeaderFieldsTooLarge>;

		// 500 series
		using InternalServerError     = Err_<StatusCode::InternalServerError>;
		using InternalError           = InternalServerError;
		using NotImplemented          = Err_<StatusCode::NotImplemented>;
		using BadGateway              = Err_<StatusCode::BadGateway>;
		using ServiceUnavailable      = Err_<StatusCode::ServiceUnavailable>;
		using GatewayTimeout          = Err_<StatusCode::GatewayTimeout>;
		using HTTPVersionNotSupported = Err_<StatusCode::HTTPVersionNotSupported>;
		using VariantAlsoNegotiates   = Err_<StatusCode::VariantAlsoNegotiates>;
		using InsufficientStorage     = Err_<StatusCode::InsufficientStorage>;
		using LoopDetected            = Err_<StatusCode::LoopDetected>;
		using NotExtended             = Err_<StatusCode::NotExtended>;
		using NetworkAuthenticationRequired = Err_<StatusCode::NetworkAuthenticationRequired>;
		
	}


	/*
		An exception carrying a status.
	*/
	class StatusException : public std::exception
	{
	public:
		const Status      status;
		const std::string message;

	public:
		StatusException(Status _status)                          : status(_status), message(_status.toString()) {}
		StatusException(Status _status, std::string _message)    : status(_status), message(std::move(_message)) {}

		inline const char *what() const override    {return message.c_str();}
	};



	/*
		MsgError and MsgException represent errors parsing or writing messages
			in the HTTP-like format used by Telling.
	*/

	class MsgError
	{
	public:
		enum ERROR
		{
			SUCCESS              = 0,
			HEADER_INCOMPLETE    = 1,
			HEADER_MALFORMED     = 2,
			HEADER_TOO_BIG       = 3,
			START_LINE_MALFORMED = 4,
			ALREADY_WRITTEN      = 5,
			UNKNOWN_PROTOCOL     = 6,
		};
	};

	using MSG_ERROR = MsgError::ERROR;


	class MsgException : public ReplyableException, public MsgError
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
			case HEADER_TOO_BIG:       return "The message header is too large (>64KiB).";
			case START_LINE_MALFORMED: return "The message's start line is malformed.";
			case ALREADY_WRITTEN:      return "The message's header has already been written.";
			case UNKNOWN_PROTOCOL:     return "The protocol is not supported.";
			default:                   return "An unknown error occurred while parsing the message.";
			}
		}

		/*
			Generate a reply message describing the error.
		*/
		Status replyStatus() const noexcept override
		{
			switch (error)
			{
			case SUCCESS:          return StatusCode::OK;
			case ALREADY_WRITTEN:  return StatusCode::InternalServerError;
			case HEADER_TOO_BIG:   return StatusCode::RequestHeaderFieldsTooLarge;
			case UNKNOWN_PROTOCOL: return StatusCode::HTTPVersionNotSupported;
			default:               return StatusCode::BadRequest;
			}
		}

		nng::msg replyWithError(std::string_view error_context) const override;


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