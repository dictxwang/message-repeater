#ifndef _GLOBAL_CONTEXT_H_
#define _GLOBAL_CONTEXT_H_

#include "config/repeater_config.h"
#include "message_container.h"

using namespace std;

namespace repeater {

    class GlobalContext {
    public:
        GlobalContext() {};
        ~GlobalContext() {};
    
    private:
        shared_ptr<MessageCircleComposite> message_circle_composite_;

    public:
        void init(RepeaterConfig& config);
        MessageCircleComposite& get_message_circle_composite();
    };
}
#endif