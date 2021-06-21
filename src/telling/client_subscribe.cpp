#include <telling/client_subscribe.h>
#include <nngpp/protocol/sub0.h>


using namespace telling;


/*
	Subscribe implementation
*/

void Subscribe::subscribe(std::string_view topic)
{
	recv_ctx().set_opt(NNG_OPT_SUB_SUBSCRIBE,
		nng::view((void*) topic.data(), topic.size()));
}
void Subscribe::unsubscribe(std::string_view topic)
{
	recv_ctx().set_opt(NNG_OPT_SUB_UNSUBSCRIBE,
		nng::view((void*) topic.data(), topic.size()));
}
