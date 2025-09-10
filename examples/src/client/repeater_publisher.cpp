#include "repeater_publisher.h"

using namespace std;

namespace repeater_client {

    bool RepeaterPublisher::isConnected(){
        return this->is_connected_;
    }

    bool RepeaterPublisher::createConnection() {

        if (this->is_connected_) {
            return true;
        }

        struct sockaddr_in server_address;
        memset(&server_address, 0, sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;
        server_address.sin_port = htons(this->server_port_);

        // Convert IP address from string to binary form
        if (inet_pton(AF_INET, this->server_ip_.c_str(), &server_address.sin_addr) <= 0) {
            return false;
        }

        // 1. Create client socket
        int client_fd = 0;
        if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            return false;
        }

        // 2. Connect to the server
        if (connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
            close(client_fd);
            return false;
        }

        this->client_fd_ = client_fd;
        this->is_connected_ = true;

        // start ping thread
        thread ping_thread([this] {
            bool is_disconnected = false;

            while (true) {
                for (int i = 0; i < 20; i++) {
                    this_thread::sleep_for(chrono::milliseconds(500));
                    if (!this->is_connected_) {
                        is_disconnected = true;
                        break;
                    }
                }
                if (is_disconnected) {
                    break;
                }

                // send ping
                bool send_result = send_socket_data(this->client_fd_, MESSAGE_OP_TOPIC_PING, "ok");
                if (!send_result) {
                    this->is_connected_ = false;
                    break;
                }
                
                // read pong
                auto frame = read_socket_frame(this->client_fd_);
                if (!frame.has_value()) {
                    this->is_connected_ = false;
                    break;
                }
                if (frame.value().first == MESSAGE_OP_TOPIC_PONG) {
                    // receive pong
                }
            }
        });
        ping_thread.detach();

        return true;
    }

    bool RepeaterPublisher::sendMessage(const string &topic, const string &message) {
        if (!this->is_connected_) {
            return false;
        }
        if (topic.size() == 0 || message.size() == 0) {
            return false;
        }

        return send_socket_data(this->client_fd_, topic, message);
    }
}