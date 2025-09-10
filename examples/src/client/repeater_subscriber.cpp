#include "repeater_subscriber.h"

namespace repeater_client {

    bool RepeaterSubscriber::isConnected() {
        return this->is_connected_;
    }

    bool RepeaterSubscriber::isSubscribed() {
        return this->is_subscribed_;
    }

    bool RepeaterSubscriber::createConnection() {

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

        return true;
    }

    bool RepeaterSubscriber::subscribe(const vector<string> &topics) {

        if (!this->is_connected_) {
            return false;
        }
        if (topics.size() == 0) {
            return false;
        }
        if (this->is_subscribed_) {
            return true;
        }

        string topics_json = "[";
        for (size_t i = 0; i < topics.size(); i++) {
            topics_json += "\"" + topics[i] + "\"";
            if (i < topics.size() - 1) {
                topics_json += ",";
            }
        }
        topics_json += "]";
        string message = "{";
        message += "\"topics\":";
        message += topics_json;
        message += "}";

        if (!send_socket_data(this->client_fd_, MESSAGE_OP_TOPIC_SUBSCRIBE, message)) {
            return false;
        }

        optional<pair<string, string>> frame = read_socket_frame(this->client_fd_);
        if (!frame.has_value()) {
            return false;
        }
        if (frame.value().first != MESSAGE_OP_TOPIC_SUBSCRIBE) {
            return false;
        }
        if (frame.value().second.find("ok") == std::string::npos) {
            return false;
        } else {
            this->is_subscribed_ = true;
            return true;
        }
    }

    optional<pair<string, string>> RepeaterSubscriber::readMessage() {
        return read_socket_frame(this->client_fd_);
    }
}