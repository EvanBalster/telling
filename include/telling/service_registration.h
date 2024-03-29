#pragma once


#include <string_view>
#include "async_loop.h"
#include "client_request.h"



namespace telling
{
	/*
		Used to register services with a Server in the same process.
	*/
	class Registration
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
		Registration(
			std::string_view servicePath,
			std::string_view servicePath_alias = std::string_view(),
			std::string_view serverID          = DefaultServerID());
		~Registration();

		/*
			Check registration status.
		*/
		STATUS                status()     const noexcept;
		const nng::exception &exception()  const noexcept;
		bool                  isWorking () const noexcept    {auto s=status(); return s==INITIAL || s==REQUESTED;}
		bool                  isRegistered() const noexcept    {return status() == ENLISTED;}


	public:
		class Delegate;
		std::shared_ptr<Delegate> delegate;
		Request                   requester;
	};
}
