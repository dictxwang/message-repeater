#ifndef _SUBSCRIBER_ACCEPTOR_H_
#define _SUBSCRIBER_ACCEPTOR_H_

#include <iostream>
#include <set>
#include "connection/acceptor.h"
#include "combiner/message_event.h"
#include "json/json.h"

namespace subscriber {

    struct ConnectionDetectingArguments {
        shared_ptr<repeater::EventLoopWorker> eventLoop;
        int client_fd;
        string client_ip;
        int client_port;
        shared_ptr<bool> connection_alived;
        bool detecting_finished;
    };

    class SubscriberBootstrap : public connection::AbstractBootstrap {
    public:
        SubscriberBootstrap() {};
        ~SubscriberBootstrap() {};
    
    private:
        shared_mutex rw_lock_;
        unordered_map<string, bool> connection_subscribed_;
        unordered_map<string, shared_ptr<repeater::EventLoopWorker>> connection_event_loop_map_;
        unordered_map<string, shared_ptr<ConnectionDetectingArguments>> connection_detecting_args_map_;
        unordered_map<string, vector<string>> topic_connection_map_;


    public:
        void startEventLoopForAcceptHandle(repeater::GlobalContext &context);
        void startEventLoopForDispatching(repeater::GlobalContext &context);

    protected:
        void acceptHandle(repeater::RepeaterConfig &config, repeater::GlobalContext &context, int client_fd, string client_ip, int client_port);
        void clearConnectionResource(repeater::GlobalContext &context, string client_ip, int client_port);
    
    private:
        // void startMessageDispatchingThread(repeater::GlobalContext &context);
        void startConnectionDetectingThread();

        void startAcceptHandleNormalWritingThread(repeater::RepeaterConfig &config, repeater::GlobalContext &context, int client_fd, string client_ip, int client_port, shared_ptr<bool> connection_alived);
        void startAcceptHandleEventLoopWritingThread(repeater::RepeaterConfig &config, repeater::GlobalContext &context, int client_fd, string client_ip, int client_port, shared_ptr<bool> connection_alived);

        void dispatchMessage(string topic);
        vector<string> parseSubscribeTopics(repeater::GlobalContext &context, string message_body);

        void putSubscribed(string client_ip, int client_port);
        void removeSubscribed(string client_ip, int client_port);
        bool isSubscribed(string client_ip, int client_port);

        void putConnectionDetectingArgs(string client_ip, int client_port, shared_ptr<ConnectionDetectingArguments> args);
        void putConnectionEventLoop(string client_ip, int client_port, shared_ptr<repeater::EventLoopWorker> eventWork);
        void putTopicConnection(string topic, string client_ip, int client_port);
        void releaseConnectionEventData(string client_ip, int client_port);
    };

    struct WritingEventWorkArguments {
        shared_ptr<repeater::EventLoopWorker> eventLoop;
        SubscriberBootstrap *subscriber;
        int client_fd;
        string client_ip;
        int client_port;
        repeater::RepeaterConfig &config;
        repeater::GlobalContext &context;
        shared_ptr<bool> connection_alived;
        shared_ptr<repeater::ConsumeRecord> consumeRecord;
        unordered_map<string, bool> circleFirstRead;
    };

    struct DispatchingEventWorkArguments {
        shared_ptr<repeater::EventLoopWorker> eventLoop;
        SubscriberBootstrap *subscriber;
        repeater::GlobalContext &context;
    };
}

#endif