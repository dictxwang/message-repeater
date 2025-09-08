#ifndef _SUBSCRIBER_ACCEPTOR_H_
#define _SUBSCRIBER_ACCEPTOR_H_

#include "connection/acceptor.h"
#include <iostream>

namespace subscriber {

    class SubscriberBootstrap : public connection::AbstractBootstrap {
    public:
        SubscriberBootstrap() {};
        ~SubscriberBootstrap() {};
    
    protected:
        void acceptHandle(repeater::RepeaterConfig &config, repeater::GlobalContext &context, int client_fd, string client_ip, int client_port);
        void clearConnectionResource(repeater::GlobalContext &context, string client_ip, int client_port);
    };
}

#endif