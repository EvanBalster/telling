#pragma once


#include <rapidjson/document.h>

#include <telling/msg_view.h>
#include <telling/service.h>


namespace telling { namespace jsonapi
{
	/*
		A service for types which live in a common container.
	*/
	class Service_TypeCollection : public telling::Service_Async
	{
	public:
		class Reactor : public telling::Reactor
		{
		public:
			virtual ~Reactor();


		protected:
			Methods       allowed() const noexcept override
			{
				return MethodCode::GET
					+  MethodCode::HEAD
					+  MethodCode::OPTIONS
					+  MethodCode::POST;
			}

			Directive recv_get    (QueryID id, const MsgView::Request &req, nng::msg &&msg) override;
			Directive recv_head   (QueryID id, const MsgView::Request &req, nng::msg &&msg) override;

			Directive recv_post   (QueryID id, const MsgView::Request &req, nng::msg &&msg) = 0;
		};
	};

	/*
		Asynchronous service specialized for JSON API.
	*/
	class JService_Async : public telling::Service_Async
	{
	public:
		/*
			Asynchronous events are delivered to a handler.
		*/
		class Handler : public telling::Service_Async::Handler
		{
		protected:
			Methods allowed;


		public:
			Handler() {}
			virtual ~Handler() {}


			
			


		protected:
			// Implementation...
			Directive pull_recv   (            nng::msg &&msg) override
			{
				MsgView::Request req
				try
				{

				}
				auto d=recv(QueryID(0), std::move(request)); return d.directive;
			}
			Directive request_recv(QueryID id, nng::msg &&msg) override
			{
				return recv(id, std::move(request));
			}
		};
	};
}}
