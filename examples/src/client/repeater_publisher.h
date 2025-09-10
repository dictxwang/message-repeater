#ifndef _REPEATER_PUBLISHER_H_
#define _REPEATER_PUBLISHER_H_

#include "util.h"
#include "constant.h"
#include <thread>
#include <chrono>

using namespace std;

namespace repeater_client {

    class RepeaterPublisher {
    public:
        RepeaterPublisher(string serverIP, int serverPort) {
            this->server_ip_ = serverIP;
            this->server_port_ = serverPort;
            this->is_connected_ = false;
        }
        ~RepeaterPublisher() {}
    
    private:
        string server_ip_;
        int server_port_;
        bool is_connected_;
        int client_fd_;
    
    private:
        void closeConnection();

    public:
        bool isConnected();
        bool createConnection();
        bool sendMessage(const string &topic, const string &message);
    };
}

#endif