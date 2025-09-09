#include "global_context.h"

using namespace std;

namespace repeater {

    void GlobalContext::init(RepeaterConfig& config) {

        for (string topic : config.allown_topics) {
            if (topic == "*") {
                this->allown_all_topics = true;
                continue;
            }
            if (!this->is_reserved_topic(topic)) {
                this->allown_topics.insert(topic);
            }
        }

        this->message_circle_composite_ = std::make_shared<MessageCircleComposite>();
        this->message_circle_composite_->init(config.max_topic_number);

        this->consume_record_composite_ = std::make_shared<ConsumeRecordComposite>();
        this->consume_record_composite_->init(config.subscriber_max_connection * 5);

        this->enable_layer_subscribe = config.enable_layer_subscribe;
        for (string topic : config.layer_subscribe_topics) {
            if (this->is_allown_topic(topic)) {
                this->layer_subscribe_topics.push_back(topic);
            }
        }
        for (string address : config.layer_subscribe_addresses) {
            if (address != config.publisher_listen_address + ":" + std::to_string(config.publisher_listen_port)
                && address != config.subscriber_listen_address + ":" + std::to_string(config.subscriber_listen_port)) {
                this->layer_subscribe_addresses.push_back(address);
            }
        }
    }

    shared_ptr<MessageCircleComposite> GlobalContext::get_message_circle_composite() {
        return this->message_circle_composite_;
    }

    shared_ptr<ConsumeRecordComposite> GlobalContext::get_consume_record_composite() {
        return this->consume_record_composite_;
    }

    bool GlobalContext::is_allown_topic(string topic) {
        return !this->is_reserved_topic(topic) && (this->allown_all_topics || this->allown_topics.find(topic) != this->allown_topics.end());
    }

    bool GlobalContext::is_reserved_topic(string topic) {
        return topic == "*" || topic == "ping" || topic == "pong" || topic == "subscribe" || topic == "error";
    }

    bool GlobalContext::is_enable_layer_subscribe() {
        return this->enable_layer_subscribe;
    }

    vector<string> &GlobalContext::get_layer_subscribe_topics() {
        return this->layer_subscribe_topics;
    }

    vector<string> &GlobalContext::get_layer_subscribe_addresses() {
        return this->layer_subscribe_addresses;
    }
}