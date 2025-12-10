#include "repeater_config.h"

using namespace std;

namespace repeater {

    bool RepeaterConfig::loadRepeaterConfig(const char* inputfile) {
        bool loadFileResult = Config::load_config(inputfile);
        if (!loadFileResult) {
            return false;
        }

        // Parse own configuration properties
        this->process_node_name = this->doc_["process_node_name"].asString();
        this->enable_run_watchdog = this->doc_["enable_run_watchdog"].asBool();

        this->tg_bot_token = this->doc_["tg_bot_token"].asString();
        this->tg_chat_id = this->doc_["tg_chat_id"].asInt64();
        this->tg_send_message = this->doc_["tg_send_message"].asBool();

        this->max_topic_number = this->doc_["max_topic_number"].asInt();
        this->max_topic_circle_size = this->doc_["max_topic_circle_size"].asInt();
        this->max_message_body_size = this->doc_["max_message_body_size"].asInt();
        this->max_connection_idle_second = this->doc_["max_connection_idle_second"].asInt();

        for (Json::Value topic : this->doc_["allown_topics"]) {
            this->allown_topics.push_back(topic.asString());
        }
        
        this->socket_write_timeout_second = this->doc_["socket_write_timeout_second"].asInt();

        this->enable_layer_subscribe = this->doc_["enable_layer_subscribe"].asBool();
        for (Json::Value addr : this->doc_["layer_subscribe_addresses"]) {
            this->layer_subscribe_addresses.push_back(addr.asString());
        }
        for (Json::Value topic : this->doc_["layer_subscribe_topics"]) {
            this->layer_subscribe_topics.push_back(topic.asString());
        }

        this->disable_accept_publisher = this->doc_["disable_accept_publisher"].asBool();
        this->publisher_listen_address = this->doc_["publisher_listen_address"].asString();
        this->publisher_listen_port = this->doc_["publisher_listen_port"].asInt();
        this->publisher_max_connection = this->doc_["publisher_max_connection"].asInt();

        this->subscriber_enable_event_loop = this->doc_["subscriber_enable_event_loop"].asBool();
        this->subscriber_listen_address = this->doc_["subscriber_listen_address"].asString();
        this->subscriber_listen_port = this->doc_["subscriber_listen_port"].asInt();
        this->subscriber_max_connection = this->doc_["subscriber_max_connection"].asInt();

        return true;
    }
}