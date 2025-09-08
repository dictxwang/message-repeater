#ifndef _GLOBAL_CONTEXT_H_
#define _GLOBAL_CONTEXT_H_

#include <set>
#include "config/repeater_config.h"
#include "message_container.h"

using namespace std;

namespace repeater {

    class GlobalContext {
    public:
        GlobalContext() {};
        ~GlobalContext() {};
    
    private:
        set<string> allown_topics;
        bool allown_all_topics;
        shared_ptr<MessageCircleComposite> message_circle_composite_;
        shared_ptr<ConsumeRecordComposite> consume_record_composite_;

    public:
        void init(RepeaterConfig& config);
        shared_ptr<MessageCircleComposite> get_message_circle_composite();
        shared_ptr<ConsumeRecordComposite> get_consume_record_composite();
        bool is_allown_topic(string topic);
    };
}
#endif