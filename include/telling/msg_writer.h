#pragma once


#include <sstream>
#include <nngpp/msg.h>

#include "msg_util.h"
#include "msg_status.h"
#include "msg_method.h"


namespace telling
{
	class MsgWriter
	{
	public:
		/*
			STEP 1: call one of these methods to begin the message.
		*/
		void startRequest (std::string_view uri, Method method = MethodCode::GET);
		void startReply   (                      Status status = StatusCode::OK)            {startReply   (     status, status.reasonPhrase());}
		void startReply   (                      Status status, std::string_view reason);
		void startBulletin(std::string_view uri, Status status = StatusCode::OK)            {startBulletin(uri, status, status.reasonPhrase());}
		void startBulletin(std::string_view uri, Status status, std::string_view reason);

		/*
			Construct a MsgWriter with STEP 1 completed.
		*/
		static MsgWriter Request (std::string_view uri, Method method = MethodCode::GET)           {MsgWriter w; w.startRequest(uri, method);          return w;}
		static MsgWriter Reply   (                      Status status = StatusCode::OK)            {MsgWriter w; w.startReply(status);                 return w;}
		static MsgWriter Reply   (                      Status status, std::string_view reason)    {MsgWriter w; w.startReply(status, reason);         return w;}
		static MsgWriter Bulletin(std::string_view uri, Status status = StatusCode::OK)            {MsgWriter w; w.startBulletin(uri, status);         return w;}
		static MsgWriter Bulletin(std::string_view uri, Status status, std::string_view reason)    {MsgWriter w; w.startBulletin(uri, status, reason); return w;}

		/*
			STEP 2: write as many headers as desired.
		*/
		//void addContentLength();
		void writeHeader(std::string_view name, std::string_view value);


		/*
			STEP 3: append body data as desired.
		*/
		void writeData(nng::view        data);
		void writeData(std::string_view text);
		void writeData(const char      *cStr)    {writeData(std::string_view(cStr));}


		/*
			STEP 4: release the composed message.
				Afterwards, MsgWriter is "reset", ready to write a new message.
		*/
		nng::msg &&release();


		/*
			OPTIONAL: set the NNG header in the held message.
				This 
		*/
		void setNNGHeader(nng::view data);


	private:
		nng::msg msg;
		size_t dataOffset = 0;
		size_t contentLengthOffset = 0;

		void _startMsg();
		void _autoCloseHeaders();
		void _append(const std::string_view &);
		void _append(char);
		void _newline();
	};
}