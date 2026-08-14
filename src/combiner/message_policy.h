#ifndef _MESSAGE_POLICY_H_
#define _MESSAGE_POLICY_H_

namespace repeater {

    enum class SubscriberOverrunPolicy {
        Latest,
        Oldest,
        Disconnect
    };

}

#endif
