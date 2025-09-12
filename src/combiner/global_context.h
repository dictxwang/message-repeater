#ifndef _GLOBAL_CONTEXT_H_
#define _GLOBAL_CONTEXT_H_

#include <set>
#include <vector>
#include "config/repeater_config.h"
#include "message_container.h"
#include "tgbot/api.h"

using namespace std;

namespace repeater {

    class GlobalContext {
    public:
        GlobalContext() {};
        ~GlobalContext() {};
    
    private:
        tgbot::TgApi tg_bot;
        set<string> allown_topics;
        bool allown_all_topics = false;
        shared_ptr<MessageCircleComposite> message_circle_composite_;
        shared_ptr<ConsumeRecordComposite> consume_record_composite_;

        bool enable_layer_subscribe;
        vector<string> layer_subscribe_topics;
        vector<string> layer_subscribe_addresses;

    public:
        void init(RepeaterConfig& config);
        
        tgbot::TgApi& get_tg_bot();
        shared_ptr<MessageCircleComposite> get_message_circle_composite();
        shared_ptr<ConsumeRecordComposite> get_consume_record_composite();
        bool is_allown_topic(string topic);
        bool is_reserved_topic(string topic);
        
        bool is_enable_layer_subscribe();
        vector<string> &get_layer_subscribe_topics();
        vector<string> &get_layer_subscribe_addresses();
    };
}
#endif