#pragma once


#include <nngpp/msg.h>
#include <nngpp/http/req.h>
#include <nngpp/http/res.h>


namespace telling
{
	/*
		Convert between NNG messages and HTTP messages.
	*/
	nng::msg MakeMsg(const nng::http::req &req);
	nng::msg MakeMsg(const nng::http::res &res);

	/*
		Convert NNG messages to HTTP messages.
			May throw MsgException if the message is not correctly-formatted.
	*/
	nng::http::req MakeHttpReq(const nng::msg &req);
	nng::http::res MakeHttpRes(const nng::msg &res);
}