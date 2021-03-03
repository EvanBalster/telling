#pragma once


#include <telling/service.h>


namespace telling_test
{
	using namespace telling;

	class Service_Test
	{
	public:
		class Receiver : public Service_Async::Handler
		{
		public:
			Receiver(Service_Test *service)
		};

	public:
		Service_Async service;
	};
}