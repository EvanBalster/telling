#pragma once


#include <cstdint>
#include <string_view>

#include <nngpp/view.h>


namespace telling
{
	/*
		A very small POD type describing the structure of a Telling message.
	*/
	class MsgLayout
	{
	public:
		enum
		{
			// Guidelines for start-line elements
			MAX_METHOD_LENGTH   = 14,
			MAX_URI_LENGTH      = 65024,
			MAX_PROTOCOL_LENGTH = 16,
			MAX_STATUS_LENGTH   = 3,
			MAX_REASON_LENGTH   = 128,
			MAX_HEADERS_LENGTH  = 65535
		};


	public:
		struct HeadRange {size_t start, length;};

		enum class TYPE : int16_t
		{
			UNKNOWN  = -1,
			REPLY   = 0, // (don't change these integer values)
			REPORT  = 1, // (they correspond to protocol's position in the start-line)
			REQUEST = 2, // (they're selected for a parsing trick)

			MASK_TYPE = 0x0F,
			FLAG_HEADER_ONLY = 0x10,
		};

		// Parse a message.
		void _parse_msg(nng::view, TYPE = TYPE::UNKNOWN);
		void _parse_reset() noexcept    {*this = {};}

		// Classify message type.
		TYPE _type() const
		{
			if (_has_method())   return TYPE::REQUEST; // Only requests have method
			if (_has_uri())      return TYPE::REPORT;  // Only reports have URI but no method
			if (_has_protocol()) return TYPE::REPLY;   // Only replies have status but no URI
			return TYPE::UNKNOWN;
		}

		HeadRange _startLine() const noexcept    {return {0u, _sl_len};}
		HeadRange _headers  () const noexcept    {size_t hb = _p_headers(); return {hb, _p_body-hb};}

		HeadRange _method  () const noexcept    {return {size_t(0u),                  size_t(_uri_pos-bool(_uri_pos))};}
		HeadRange _uri     () const noexcept    {return {size_t(_uri_pos),            size_t(_has_uri() ? _sl_len-_uri_pos-_prt_rpos-bool(_prt_rpos) : 0u)};}
		HeadRange _protocol() const noexcept    {return {size_t(_sl_len-_prt_rpos),   size_t(_prt_rpos ? (_prt_rpos-_sts_rpos-bool(_sts_rpos)) : 0u)};}
		HeadRange _status  () const noexcept    {return {size_t(_sl_len-_sts_rpos),   size_t(_sts_rpos ? 3u : 0u)};}
		HeadRange _reason  () const noexcept    {return {size_t(_sl_len+4-_sts_rpos), size_t((_sts_rpos>4) ? _sts_rpos-4 : 0)};}

		bool _has_method()   const noexcept    {return _uri_pos;}
		bool _has_uri()      const noexcept    {return _prt_rpos < _sl_len;}
		bool _has_protocol() const noexcept    {return _prt_rpos >= _sts_rpos+1;}
		bool _has_status()   const noexcept    {return _sts_rpos >= 3;}
		bool _has_reason()   const noexcept    {return _sts_rpos > 4;}


	public: // 64-bit representation of message structure

		// Offset of message body and headers
		uint16_t _p_body;
		size_t    _p_headers() const noexcept    {return _sl_len + _sl_nl;}

		// Design start-line.
		void _setStartLine(
			std::string_view  lineWithEol,
			TYPE              type,
			const char       *second_elem = nullptr,
			const char       *third_elem  = nullptr,
			const char       *fourth_elem = nullptr);

	private:
		// Lengths of start-line components, including space where suffixed
		uint16_t _sl_len;
		uint8_t _uri_pos, _prt_rpos, _sts_rpos, _sl_nl;
		// TODO: _prt_rpos is 0 for "GET x "
	};
}