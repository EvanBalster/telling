#pragma once


#include <string_view>
#include "async_query.h"
#include "client_request.h"



namespace telling
{
	/*
		Used to enlist services with a Server in the same process.
	*/
	class Enlistment
	{
	public:
		enum STATUS
		{
			INITIAL   = 0,
			REQUESTED = 1,
			ENLISTED  = 2,
			FAILED   = -1,
		};


	public:
		Enlistment(
			std::string_view serverID,
			std::string_view serviceURI);
		Enlistment(
			std::string_view serverID,
			std::string_view serviceURI,
			std::string_view serviceURI_enlist_as);
		~Enlistment();

		/*
			Check enlistment status.
		*/
		STATUS                status()     const noexcept;
		const nng::exception &exception()  const noexcept;
		bool                  isWorking () const noexcept    {auto s=status(); return s==INITIAL || s==REQUESTED;}
		bool                  isEnlisted() const noexcept    {return status() == ENLISTED;}


	public:
		class Delegate;
		std::shared_ptr<AsyncQuery> delegate;
		client::Request_Async       requester;
	};
}
