#include "publisher_acceptor.h"

namespace publisher {

    void PublisherBootstrap::acceptHandle(repeater::RepeaterConfig &config, repeater::GlobalContext &context, int client_fd) {
        
        const size_t HEADER_SIZE = 4;
        const size_t MAX_MESSAGE_SIZE = 65536;
        
        while (true) {
            // Step1: read header of type
            uint32_t message_type = 0;
            ssize_t bytes_received = recv(client_fd, &message_type, HEADER_SIZE, MSG_WAITALL);
            if (bytes_received != HEADER_SIZE) {
                if (bytes_received == 0) {
                    err_log("client of {} disconnected", this->role_);
                } else {
                    err_log("fail to read header of type from client of {}", this->role_);
                }
                break;
            }
            message_type = ntohl(message_type);

            #ifdef OPEN_STD_DEBUG_LOG
                std::cout << this->role_ << " receive message type: " << message_type << std::endl;
            #endif

            // Step2: read header of main data length
            uint32_t message_length = 0;
            bytes_received = recv(client_fd, &message_length, HEADER_SIZE, MSG_WAITALL);
            if (bytes_received != HEADER_SIZE) {
                if (bytes_received == 0) {
                    err_log("client of {} disconnected", this->role_);
                } else {
                    err_log("fail to read header of length from client of {}", this->role_);
                }
                break;
            }
            message_length = ntohl(message_length);
            #ifdef OPEN_STD_DEBUG_LOG
                std::cout << this->role_ << " receive message length: " << message_length << std::endl;
            #endif

            if (message_length > MAX_MESSAGE_SIZE) {
                err_log("message from client of {} is too large, which length is {}", this->role_, message_length);
                break;
            }

            // Step3: read main data
            std::vector<char> message_buffer(message_length + 1);
            bytes_received = recv(client_fd, message_buffer.data(), message_length, MSG_WAITALL);
            
            if (bytes_received != message_length) {
                err_log("fail to read complete message from client of {}", this->role_);
                break;
            }

            message_buffer[message_length] = '\0';

            #ifdef OPEN_STD_DEBUG_LOG
                std::cout << this->role_ << " receive message: " << message_type << "," << message_length << "," << message_buffer.data() << std::endl;
            #endif

            // Step4: process main data by message type
            if (message_type == connection::MESSAGE_TYPE_PING) {
                // TODO
            } else if (message_type == connection::MESSAGE_TYPE_NORMAL_DATA) {
                // TODO
            } else {
                warn_log("{} receive not supported message type {}", this->role_, message_type);
            }
        }

        close(client_fd);
    }
}