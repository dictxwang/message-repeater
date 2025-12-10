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
        string process_node_name;
        bool enable_run_watchdog;

        string tg_bot_token;
        int64_t tg_chat_id;
        bool tg_send_message;

        int max_topic_number;
        int max_topic_circle_size;
        int max_message_body_size;
        int max_connection_idle_second;

        vector<string> allown_topics;
        int socket_write_timeout_second;

        bool enable_layer_subscribe;
        vector<string> layer_subscribe_addresses;
        vector<string> layer_subscribe_topics;

        bool disable_accept_publisher;
        string publisher_listen_address;
        int publisher_listen_port;
        int publisher_max_connection;
        
        bool subscriber_enable_event_loop;
        string subscriber_listen_address;
        int subscriber_listen_port;
        int subscriber_max_connection;
    };
}

#endif