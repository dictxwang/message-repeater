#ifndef _REPEATER_SUBSCRIBER_H_
#define _REPEATER_SUBSCRIBER_H_

#include "util.h"
#include "constant.h"
#include <cstring>
#include <thread>
#include <chrono>

using namespace std;

namespace repeater_client {

    class RepeaterSubscriber {
    public:
        RepeaterSubscriber(string serverIP, int serverPort) {
            this->server_ip_ = serverIP;
            this->server_port_ = serverPort;
            this->is_connected_ = false;
            this->is_subscribed_ = false;
        }
        ~RepeaterSubscriber() {}
    
    private:
        string server_ip_;
        int server_port_;
        bool is_connected_;
        bool is_subscribed_;
        int client_fd_;
    
    private:
        void closeConnection();
    
    public:
        bool isConnected();
        bool isSubscribed();
        bool createConnection();
        bool subscribe(const vector<string> &topics);
        optional<pair<string, string>> readMessage();
    };
}

#endif