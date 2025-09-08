#ifndef _CONNECTION_ACCEPTOR_H_
#define _CONNECTION_ACCEPTOR_H_

#include <string>
#include <mutex>
#include <shared_mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdint>
#include <unordered_map>
#include <thread>
#include <iostream>
#include <cstring>
#include "logger/logger.h"
#include "config/repeater_config.h"
#include "combiner/global_context.h"
#include "util/common_tool.h"

using namespace std;

namespace connection {

    const string MESSAGE_OP_TOPIC_PING = "ping";
    const string MESSAGE_OP_TOPIC_PONG = "pong";
    const string MESSAGE_OP_TOPIC_SUBSCRIBE = "subscribe";

    const string SERVER_ROLE_PUBLISHER = "publisher";
    const string SERVER_ROLE_SUBSCRIBER = "subscriber";

    struct ConnectionEntity {
        string clientIP;
        int clientPort;
        int clientFd;
        uint64_t latestHeartbeat;
    };

    class AbstractBootstrap {
    public:
        AbstractBootstrap() {};
        virtual ~AbstractBootstrap() {};

    protected:
        string role_;
    private:
        int server_fd_;
        string listen_address_;
        int listen_port_;
        int max_connection_;

        unordered_map<string, ConnectionEntity> client_connections_;

        shared_mutex rw_lock_;
    
    private:
        void startAliveDetection(repeater::RepeaterConfig &config, repeater::GlobalContext context);
        void startAccept(repeater::RepeaterConfig &config, repeater::GlobalContext &context);

    protected:
        bool sendSocketData(int client_fd, string topic, string message);
        void refreshKeepAlive(string client_ip, int client_port);
        virtual void acceptHandle(repeater::RepeaterConfig &config, repeater::GlobalContext &context, int client_fd, string client_ip, int client_port);

    public:
        void init(string role, string listen_address, int listen_port, int max_connection);
        void start(repeater::RepeaterConfig &config, repeater::GlobalContext &context);
    };
}

#endif