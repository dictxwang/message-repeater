#include "util.h"

namespace repeater_client {

    bool send_socket_data(const int client_fd, const string &topic, const string &message) {

        vector<char> buffer;
        // Serialize string length
        uint32_t topic_length = htonl(topic.length());
        buffer.insert(buffer.end(), reinterpret_cast<const char*>(&topic_length),
                    reinterpret_cast<const char*>(&topic_length) + sizeof(topic_length));
        // Serialize string data
        buffer.insert(buffer.end(), topic.begin(), topic.end());

        // Serialize string length
        uint32_t message_length = htonl(message.length());
        buffer.insert(buffer.end(), reinterpret_cast<const char*>(&message_length),
                    reinterpret_cast<const char*>(&message_length) + sizeof(message_length));
        // Serialize string data
        buffer.insert(buffer.end(), message.begin(), message.end());

        ssize_t bytes_sent = 0;
        
        try {
            // bytes_sent = send(client_fd, buffer.data(), buffer.size(), 0);
            bytes_sent = send(client_fd, buffer.data(), buffer.size(), MSG_NOSIGNAL);
        } catch (std::exception &e) {
            return false;
        }
        
        if (bytes_sent < 0) {
            return false;
        } else if (bytes_sent == 0) {
            return false;
        }
        return true;
    }

    optional<pair<string, string>> read_socket_frame(const int client_fd) {

        size_t HEADER_SIZE = 4;
        size_t MAX_MESSAGE_SIZE = 65536;

        // Step1: read header of topic length
        uint32_t topic_length = 0;
        ssize_t bytes_received = recv(client_fd, &topic_length, HEADER_SIZE, MSG_WAITALL);
        if (bytes_received != HEADER_SIZE) {
            if (bytes_received == 0) {
            } else {
            }
            return nullopt;
        }
        topic_length = ntohl(topic_length);

        // Step2: read header of topic name
        std::vector<char> topic_buffer(topic_length + 1);
        bytes_received = recv(client_fd, topic_buffer.data(), topic_length, MSG_WAITALL);
        if (bytes_received != topic_length) {
            return nullopt;
        }

        // Step3: read header of main data length
        uint32_t message_length = 0;
        bytes_received = recv(client_fd, &message_length, HEADER_SIZE, MSG_WAITALL);
        if (bytes_received != HEADER_SIZE) {
            if (bytes_received == 0) {
            } else {
            }
            return nullopt;
        }
        message_length = ntohl(message_length);

        if (message_length > MAX_MESSAGE_SIZE) {
            return nullopt;
        }

        // Step4: read main data
        std::vector<char> message_buffer(message_length + 1);
        bytes_received = recv(client_fd, message_buffer.data(), message_length, MSG_WAITALL);
        
        if (bytes_received != message_length) {
            return nullopt;
        }

        message_buffer[message_length] = '\0';
        
        return std::make_pair<string, string>(std::string(topic_buffer.data()), std::string(message_buffer.data()));
    }
}