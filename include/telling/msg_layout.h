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
			if (_mth_sp) return TYPE::REQUEST; // Only requests have method
			if (_uri_sp) return TYPE::REPORT;  // Only reports have URI but no method
			if (_sts_sp) return TYPE::REPLY;   // Only replies have status but no URI
			return TYPE::UNKNOWN;
		}

		HeadRange _start_nl () const noexcept    {return {0u, _p_headers()};}
		HeadRange _headers  () const noexcept    {size_t hb = _p_headers(); return {hb, _p_body-hb};}

		HeadRange _method   () const noexcept    {return {size_t(0u),                      size_t(_mth_sp ? (_mth_sp-1u) : 0u)};}
		HeadRange _uri      () const noexcept    {return {size_t(_mth_sp),                 size_t(_uri_sp ? (_uri_sp-1u) : 0u)};}
		HeadRange _protocol () const noexcept    {return {size_t(_mth_sp+_uri_sp),         size_t(_prt_sp ? (_prt_sp-1u) : 0u)};}
		HeadRange _status   () const noexcept    {return {size_t(_mth_sp+_uri_sp+_prt_sp), size_t(_sts_sp ? (_sts_sp-1u) : 0u)};}
		HeadRange _reason_nl() const noexcept    {return {size_t(_mth_sp+_uri_sp+_prt_sp+_sts_sp), _rea_nl};}


	public: // 64-bit representation of message structure

		// Offset of message body and headers
		uint16_t _p_body;
		size_t    _p_headers() const noexcept    {return _mth_sp+_uri_sp+_prt_sp+_sts_sp+_rea_nl;}

		// Lengths of start-line components, including space where suffixed
		uint16_t _uri_sp;
		uint8_t  _mth_sp, _prt_sp, _sts_sp, _rea_nl;

		
	};
}