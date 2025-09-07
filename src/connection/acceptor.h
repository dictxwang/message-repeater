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
#include "logger/logger.h"
#include "config/repeater_config.h"
#include "combiner/global_context.h"
#include "util/common_tool.h"

using namespace std;

namespace connection {

    const uint32_t MESSAGE_TYPE_PING = 101;
    const uint32_t MESSAGE_TYPE_PONG = 102;
    const uint32_t MESSAGE_TYPE_NORMAL_DATA = 201;

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
        virtual void acceptHandle(repeater::RepeaterConfig &config, repeater::GlobalContext &context, int client_fd);

    public:
        void init(string role, string listen_address, int listen_port, int max_connection);
        void start(repeater::RepeaterConfig &config, repeater::GlobalContext &context);
    };
}

#endif