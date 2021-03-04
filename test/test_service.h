#pragma once


#include <telling/service.h>


namespace telling_test
{
	using namespace telling;

	class Service_Test : public Service_Async
	{
	public:
		class Receiver : public Service_Async::Handler
		{
		public:
			std::mutex    mtx;
			Service_Test *service;

		public:
			Receiver(Service_Test &_service) : service(&_service)
			{

			}

			SendDirective recv(QueryID queryID, nng::msg &&msg)
			{
				if (queryID)
				{

				}
			}
		};

	public:
		Service_Test() :
			Service_Async(std::make_shared<Receiver>(*this))
		{

		}
	};
}