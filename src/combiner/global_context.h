#ifndef _GLOBAL_CONTEXT_H_
#define _GLOBAL_CONTEXT_H_

#include <set>
#include <vector>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include "config/repeater_config.h"
#include "message_container.h"
#include "message_event.h"
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

        vector<string> layer_subscribe_topics;
        vector<string> layer_subscribe_addresses;

        shared_ptr<unordered_map<string, bool>> bootstrap_connections_full_status;

        queue<string> message_topics_for_event_loop;

        shared_ptr<EventLoopWorker> dispatch_event_loop_worker_;
        shared_ptr<shared_mutex> rw_lock_;

    public:
        void init(RepeaterConfig& config);

        tgbot::TgApi& get_tg_bot();
        shared_ptr<MessageCircleComposite> get_message_circle_composite();
        shared_ptr<ConsumeRecordComposite> get_consume_record_composite();
        bool is_allown_topic(string topic);
        bool is_reserved_topic(string topic);
        
        vector<string> &get_layer_subscribe_topics();
        vector<string> &get_layer_subscribe_addresses();

        void update_connections_full(string role, bool fulled);
        vector<string> get_connections_full_roles();

        shared_ptr<EventLoopWorker> get_dispatch_event_loop_worker();
        void submit_message_topic_to_event_loop(string topic);
        void notify_message_topic_to_event_loop();
        // void push_message_topic_for_event_loop(string topic);
        // vector<string> pop_message_topics_for_event_loop();
    };
}
#endif