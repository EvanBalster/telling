#pragma once


#include <rapidjson/document.h>

#include <telling/msg_view.h>
#include <telling/service.h>


namespace telling { namespace jsonapi
{
	/*
		A service for types which live in a common container.
	*/
	class Service_TypeCollection : public telling::Service
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

			void recv_get    (QueryID id, const MsgView::Request &req, nng::msg &&msg) override;
			void recv_head   (QueryID id, const MsgView::Request &req, nng::msg &&msg) override;

			void recv_post   (QueryID id, const MsgView::Request &req, nng::msg &&msg) = 0;
		};
	};

	/*
		Asynchronous service specialized for JSON API.
	*/
	class JService : public telling::Service
	{
	public:
		/*
			Asynchronous events are delivered to a handler.
		*/
		class Handler : public telling::Service::Handler
		{
		protected:
			Methods allowed;


		public:
			Handler() {}
			virtual ~Handler() {}


			
			


		protected:
			// Implementation...
			void pull_recv   (            nng::msg &&msg) override
			{
				MsgView::Request req
				try
				{

				}
				auto d=recv(QueryID(0), std::move(request)); return d.directive;
			}
			void request_recv(QueryID id, nng::msg &&msg) override
			{
				return recv(id, std::move(request));
			}
		};
	};
}}
