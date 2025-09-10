#ifndef _REPEATER_CONSTANT_H_
#define _REPEATER_CONSTANT_H_

#include <string>

using namespace std;

namespace repeater_client {

    const string MESSAGE_OP_TOPIC_PING = "ping";
    const string MESSAGE_OP_TOPIC_PONG = "pong";
    const string MESSAGE_OP_TOPIC_SUBSCRIBE = "subscribe";
}
#endif