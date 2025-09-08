#include "global_context.h"

namespace repeater {

    void GlobalContext::init(RepeaterConfig& config) {
        this->message_circle_composite_ = std::make_shared<MessageCircleComposite>();
        this->message_circle_composite_->init(config.max_topic_number);
    }

    shared_ptr<MessageCircleComposite> GlobalContext::get_message_circle_composite() {
        return this->message_circle_composite_;
    }
}