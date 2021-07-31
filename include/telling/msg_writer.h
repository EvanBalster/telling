#pragma once


#include <sstream>
#include <nngpp/msg.h>
#include <nngpp/msg_iostream.h>

#include "msg_util.h"
#include "msg.h"

#include "msg_status.h"
#include "msg_method.h"
#include "msg_protocol.h"


namespace telling
{
	class MsgWriter : protected Msg
	{
	public:
		MsgWriter(MsgProtocol protocol = Telling);

		/*
			STEP 1: call one of these methods to begin the message.
		*/
		void startRequest(std::string_view uri, Method method = MethodCode::GET);
		void startReply  (                      Status status = StatusCode::OK)            {startReply (     status, status.reasonPhrase());}
		void startReply  (                      Status status, std::string_view reason);
		void startReport (std::string_view uri, Status status = StatusCode::OK)            {startReport(uri, status, status.reasonPhrase());}
		void startReport (std::string_view uri, Status status, std::string_view reason);

		/*
			STEP 2: write as many headers as desired.
		*/
		//void addContentLength();
		void writeHeader(std::string_view name, std::string_view value);

		// Specific headers...
		void writeHeader_Allow (Methods methods);
		void writeHeader_Length(size_t  maxLength = ~uint32_t(0));


		/*
			STEP 3: append body data as desired.
		*/
		template<typename C=char, class Tr=std::char_traits<C>>
		nng::basic_omsgstream<C,Tr> writeBody() noexcept    {_autoCloseHeaders(); return nng::basic_omsgstream<C,Tr>(bodyBuf<C,Tr>(std::ios::out | std::ios::binary | std::ios::ate));}


		/*
			STEP 4: release the composed message.
				Afterwards, MsgWriter is "reset", ready to write a new message.
		*/
		nng::msg release();


		/*
			OPTIONAL: set the NNG header in the held message.
				This 
		*/
		void setNNGHeader(nng::view data);


	protected:
		MsgProtocol protocol;

	private:
		struct
		{
			uint16_t lengthOffset, lengthSize;
		}
			head = {};

		void _startMsg();
		void _autoCloseHeaders();
		void _newline();
	};


	/*
		Construct a MsgWriter with STEP 1 completed.
	*/
	inline MsgWriter WriteRequest(std::string_view uri, Method method = MethodCode::GET)           {MsgWriter w;      w.startRequest(uri, method);        return w;}
	inline MsgWriter WriteReply  (                      Status status = StatusCode::OK)            {MsgWriter w;      w.startReply(status);               return w;}
	inline MsgWriter WriteReport (std::string_view uri, Status status = StatusCode::OK)            {MsgWriter w;      w.startReport(uri, status);         return w;}
	inline MsgWriter WriteReply  (                      Status status, std::string_view reason)    {MsgWriter w;      w.startReply(status, reason);       return w;}
	inline MsgWriter WriteReport (std::string_view uri, Status status, std::string_view reason)    {MsgWriter w;      w.startReport(uri, status, reason); return w;}

	inline MsgWriter HttpRequest (std::string_view uri, Method method = MethodCode::GET)           {MsgWriter w=Http; w.startRequest(uri, method);          return w;}
	inline MsgWriter HttpReply   (                      Status status = StatusCode::OK)            {MsgWriter w=Http; w.startReply(status);                 return w;}
	inline MsgWriter HttpReply   (                      Status status, std::string_view reason)    {MsgWriter w=Http; w.startReply(status, reason);         return w;}
}
