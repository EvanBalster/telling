#pragma once


#include <nngpp/msg.h>

#include "msg_view.h"


namespace telling
{
	/*
		A parsed message which is owned by this object.
	*/
	class Msg : public MsgView
	{
	public:
		class Request;
		class Reply;
		class Report;


	public:
		Msg ()            noexcept                      {}
		Msg(nng::msg &&m, TYPE type = TYPE::UNKNOWN)    : MsgView(m.release(), type) {}
		~Msg()            noexcept                      {_free();}

		/*
			Release ownership of the message.
				Afterwards, this object can't be used to access message fields.
				Consider copying to a MsgView beforehand.
		*/
		nng::msg release() noexcept    {return nng::msg(_release());}

		/*
			Clone a message without re-parsing it.
		*/
		Msg clone() const              {return Msg(*this, clone_tag{});}

		/*
			Move-construction and move-assignment.
		*/
		Msg(Msg &&o)            noexcept    : MsgView(o) {o._release();}
		Msg &operator=(Msg &&o) noexcept    {_free(); MsgView::operator=(o); o._release(); return *this;}


		/*
			Write a header to the message's end.
		*/


	protected:
		// No implicit copying, only moving.
		Msg           (const Msg&) = delete;
		void operator=(const Msg&) = delete;

		struct clone_tag {};
		struct create_tag {};

		Msg(create_tag)
		{
			msg = nng::msg(size_t(0)).release();
		}
		Msg(const Msg &o, clone_tag) : MsgView(o)
		{
			nng::msg orig(msg.get()), copy(orig);
			msg = copy;
			orig.release(); copy.release();
		}
		void     _free()    noexcept    {nng_msg *m = _release(); if (m) nng_msg_free(m);}
		nng_msg *_release() noexcept    {nng_msg *m = msg.get(); msg = nng::msg_view(); return m;}
	};


	class Msg::Request : public Msg
	{
	public:
		// Parse a Request
		Request(nng::msg &&msg)     : Msg(std::move(msg), TYPE::REQUEST) {}

		~Request() noexcept {}
		Request()  noexcept {}

		Request clone() const       {return Request(*this, clone_tag{});}

		Request(Request &&o)            noexcept    : Msg(std::move(o)) {}
		Request &operator=(Request &&o) noexcept    {Msg::operator=(std::move(o)); return *this;}

	protected:
		Request(const Request &o, clone_tag)     : Msg(o, clone_tag{}) {}
	};

	class Msg::Reply : public Msg
	{
	public:
		// Parse a Reply
		Reply(nng::msg &&msg)       : Msg(std::move(msg), TYPE::REPLY) {}

		~Reply() noexcept {}
		Reply()  noexcept {}

		Reply clone() const         {return Reply(*this, clone_tag{});}

		Reply(Reply &&o)            noexcept    : Msg(std::move(o)) {}
		Reply &operator=(Reply &&o) noexcept    {Msg::operator=(std::move(o)); return *this;}

	protected:
		Reply(const Reply &o, clone_tag)     : Msg(o, clone_tag{}) {}
	};

	class Msg::Report : public Msg
	{
	public:
		// Parse a Report
		Report(nng::msg &&msg)    : Msg(std::move(msg), TYPE::REPORT) {}

		~Report() noexcept {}
		Report()  noexcept {}

		Report clone() const      {return Report(*this, clone_tag{});}

		Report(Report &&o)            noexcept    : Msg(std::move(o)) {}
		Report &operator=(Report &&o) noexcept    {Msg::operator=(std::move(o)); return *this;}

	protected:
		Report(const Report &o, clone_tag)     : Msg(o, clone_tag{}) {}
	};
}