#ifndef _SUBSCRIBER_ACCEPTOR_H_
#define _SUBSCRIBER_ACCEPTOR_H_

#include <iostream>
#include "connection/acceptor.h"
#include "json/json.h"

namespace subscriber {

    class SubscriberBootstrap : public connection::AbstractBootstrap {
    public:
        SubscriberBootstrap() {};
        ~SubscriberBootstrap() {};
    
    private:
        unordered_map<string, bool> connection_subscribed;

    protected:
        void acceptHandle(repeater::RepeaterConfig &config, repeater::GlobalContext &context, int client_fd, string client_ip, int client_port);
        void clearConnectionResource(repeater::GlobalContext &context, string client_ip, int client_port);

        vector<string> parseSubscribeTopics(string message_body);

        void putSubscribed(string client_ip, int client_port);
        void removeSubscribed(string client_ip, int client_port);
        bool isSubscribed(string client_ip, int client_port);
    };
}

#endif