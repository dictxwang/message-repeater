#ifndef _CONFIG_AGENT_CONFIG_H_
#define _CONFIG_AGENT_CONFIG_H_

#include "config.h"
#include <vector>

using namespace std;

namespace repeater {

    class RepeaterConfig : public Config
    {
    public:
        RepeaterConfig() {}
        ~RepeaterConfig() {}

    public:
        bool loadRepeaterConfig(const char* inputfile);

    public:
        int max_topic_number;
        int max_topic_circle_size;
        int max_message_body_size;
        int max_connection_idle_second;

        int socket_write_timeout_second;

        bool enable_layer_subscribe;
        vector<string> layer_subscribe_addresses;
        vector<string> layer_subscribe_topics;

        string publisher_listen_address;
        int publisher_listen_port;
        int publisher_max_connection;
        
        string subscriber_listen_address;
        int subscriber_listen_port;
        int subscriber_max_connection;
    };
}

#endif