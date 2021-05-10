#include <telling/http.h>
#include <telling/msg_writer.h>
#include <telling/msg_view.h>


using namespace telling;


nng::msg telling::MakeMsg(const nng::http::req &req)
{
	Method method = Method::Parse(req.get_method());
	MsgWriter result = MsgWriter::Request(req.get_uri(), method);

	result.writeData(req.get_data());

	return result.release();
}
nng::msg telling::MakeMsg(const nng::http::res &res)
{
	MsgWriter result = MsgWriter::Reply(StatusCode(res.get_status()), res.get_reason());

	result.writeData(res.get_data());

	return result.release();
}

nng::http::req telling::MakeHttpReq(const nng::msg &_req)
{
	nng::http::req result;

	MsgView::Request req(_req);

	std::string tmp, tmp2;

	tmp = req.uri;
	result.set_uri(tmp.c_str());

	tmp = req.methodString;
	result.set_method(tmp.c_str());

	// TODO version?

	for (auto &header : req.headers())
	{
		tmp = header.name;
		tmp2 = header.value;
		result.add_header(tmp.c_str(), tmp2.c_str());
	}

	result.set_data(req.data());

	return result;
}
nng::http::res telling::MakeHttpRes(const nng::msg &_res)
{
	nng::http::res result;

	MsgView::Reply res(_res);

	std::string tmp, tmp2;

	tmp = res.statusString;
	result.set_status(nng::http::status(res.status().code));

	tmp = res.reason;
	result.set_reason(tmp.c_str());

	// TODO version?

	for (auto &header : res.headers())
	{
		tmp = header.name;
		tmp2 = header.value;
		result.add_header(tmp.c_str(), tmp2.c_str());
	}

	result.set_data(res.data());

	return result;
}
