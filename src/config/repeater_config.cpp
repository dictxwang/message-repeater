#include "repeater_config.h"

using namespace std;

namespace repeater {

    bool RepeaterConfig::loadRepeaterConfig(const char* inputfile) {
        bool loadFileResult = Config::load_config(inputfile);
        if (!loadFileResult) {
            return false;
        }

        // Parse own configuration properties
        this->max_topic_number = this->doc_["max_topic_number"].asInt();
        this->max_topic_queue_size = this->doc_["max_topic_queue_size"].asInt();
        this->max_connection_idle_second = this->doc_["max_connection_idle_second"].asInt();

        for (Json::Value addr : this->doc_["layer_subscribe_addresses"]) {
            this->layer_subscribe_addresses.push_back(addr.asString());
        }
        for (Json::Value topic : this->doc_["layer_subscribe_topics"]) {
            this->layer_subscribe_topics.push_back(topic.asString());
        }

        this->publisher_listen_address = this->doc_["publisher_listen_address"].asString();
        this->publisher_listen_port = this->doc_["publisher_listen_port"].asInt();
        this->publisher_max_connection = this->doc_["publisher_max_connection"].asInt();

        this->subscriber_listen_address = this->doc_["subscriber_listen_address"].asString();
        this->subscriber_listen_port = this->doc_["subscriber_listen_port"].asInt();
        this->subscriber_max_connection = this->doc_["subscriber_max_connection"].asInt();
        
        return true;
    }
}