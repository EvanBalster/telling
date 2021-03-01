#include <iostream>

#include <telling/service.h>

#include <telling/msg_writer.h>
#include <telling/msg_view.h>


using namespace telling;



Service_Box::Service_Box(std::string _uri, std::string_view serverID) :
	uri(_uri)
{
	auto base = address();

	replier  .listen(base);
	puller   .listen(base);
	publisher.listen(base);

	if (serverID.length()) serve(serverID);
}

Service_Box::~Service_Box()
{
	std::cout << __FUNCTION__ << " start" << std::endl;

	replier.close();
	puller.close();
	publisher.close();

	std::cout << __FUNCTION__ << " end" << std::endl;
}

void Service_Box::serve(std::string_view serverID)
{
	publisher.dial(HostAddress::Base::InProc(serverID));

	broadcastServiceRegistration();
}

void Service_Box::broadcastServiceRegistration()
{
	MsgWriter msg = MsgWriter::Bulletin("*services");
	msg.writeData(uri);
	msg.writeData("\r\n");

	publisher.publish(msg.release());
}