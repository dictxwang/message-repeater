#include "global_context.h"

using namespace std;

namespace repeater {

    void GlobalContext::init(RepeaterConfig& config) {

        for (string topic : config.allown_topics) {
            if (topic == "*") {
                this->allown_all_topics = true;
                continue;
            }
            this->allown_topics.insert(topic);
        }

        this->message_circle_composite_ = std::make_shared<MessageCircleComposite>();
        this->message_circle_composite_->init(config.max_topic_number);

        this->consume_record_composite_ = std::make_shared<ConsumeRecordComposite>();
        this->consume_record_composite_->init(config.subscriber_max_connection * 5);
    }

    shared_ptr<MessageCircleComposite> GlobalContext::get_message_circle_composite() {
        return this->message_circle_composite_;
    }

    shared_ptr<ConsumeRecordComposite> GlobalContext::get_consume_record_composite() {
        return this->consume_record_composite_;
    }

    bool GlobalContext::is_allown_topic(string topic) {
        return this->allown_all_topics || this->allown_topics.find(topic) != this->allown_topics.end();
    }
}