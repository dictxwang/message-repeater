#ifndef _SUBSCRIBER_ACCEPTOR_H_
#define _SUBSCRIBER_ACCEPTOR_H_

#include <iostream>
#include <set>
#include "connection/acceptor.h"
#include "combiner/message_event.h"
#include "json/json.h"

namespace subscriber {

    struct EventWorkArguments {
        repeater::EventLoopWorker* eventLoop;
        subscriber::SubscriberBootstrap* subscriber;
        int client_fd;
        string client_ip;
        int client_port;
        repeater::RepeaterConfig &config;
        repeater::GlobalContext &context;
        shared_ptr<bool> connection_alived;
        unordered_map<string, bool> firstReadCircle;
    };

    class SubscriberBootstrap : public connection::AbstractBootstrap {
    public:
        SubscriberBootstrap() {};
        ~SubscriberBootstrap() {};
    
    private:
        shared_mutex rw_lock_;
        unordered_map<string, bool> connection_subscribed_;
        unordered_map<string, repeater::EventLoopWorker*> connection_event_loop_map_;
        unordered_map<string, vector<string>> topic_connection_map_;


    public:
        void startMessageDispatch(repeater::GlobalContext &context);

    protected:
        void acceptHandle(repeater::RepeaterConfig &config, repeater::GlobalContext &context, int client_fd, string client_ip, int client_port);
        void clearConnectionResource(repeater::GlobalContext &context, string client_ip, int client_port);

        void dispatchMessage(string topic);
        vector<string> parseSubscribeTopics(repeater::GlobalContext &context, string message_body);

        void putSubscribed(string client_ip, int client_port);
        void removeSubscribed(string client_ip, int client_port);
        bool isSubscribed(string client_ip, int client_port);

        void putConnectionEventLoop(string client_ip, int client_port, repeater::EventLoopWorker *eventWork);
        void putTopicConnection(string topic, string client_ip, int client_port);
        void releaseConnectionEventData(string client_ip, int client_port);
    };
}

#endif