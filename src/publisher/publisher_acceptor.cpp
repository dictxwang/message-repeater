#include "publisher_acceptor.h"

namespace publisher {

    void PublisherBootstrap::acceptHandle(repeater::RepeaterConfig &config, repeater::GlobalContext &context, int client_fd, string client_ip, int client_port) {

        size_t HEADER_SIZE = 4;
        size_t MAX_MESSAGE_SIZE = 65536;

        while (true) {
            // Step1: read header of topic length
            uint32_t topic_length = 0;
            ssize_t bytes_received = recv(client_fd, &topic_length, HEADER_SIZE, MSG_WAITALL);
            if (bytes_received != HEADER_SIZE) {
                if (bytes_received == 0) {
                    err_log("client of {} disconnected", this->role_);
                } else {
                    err_log("fail to read header of type from client of {}", this->role_);
                }
                break;
            }
            topic_length = ntohl(topic_length);

            #ifdef OPEN_STD_DEBUG_LOG
                std::cout << this->role_ << " receive topic length: " << topic_length << std::endl;
            #endif

            // Step2: read header of topic name
            std::vector<char> topic_buffer(topic_length + 1);
            bytes_received = recv(client_fd, topic_buffer.data(), topic_length, MSG_WAITALL);
            if (bytes_received != topic_length) {
                err_log("fail to read complete topic from client of {}", this->role_);
                break;
            }

            // Step3: read header of main data length
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

            // Step4: read main data
            std::vector<char> message_buffer(message_length + 1);
            bytes_received = recv(client_fd, message_buffer.data(), message_length, MSG_WAITALL);
            
            if (bytes_received != message_length) {
                err_log("fail to read complete message from client of {}", this->role_);
                break;
            }

            message_buffer[message_length] = '\0';

            #ifdef OPEN_STD_DEBUG_LOG
                std::cout << this->role_ << " receive data: " << topic_length << "," << topic_buffer.data() << "," << message_length << "," << message_buffer.data() << std::endl;
            #endif

            // Step5: process main data by message type
            if (topic_buffer.data() == connection::MESSAGE_OP_TOPIC_PING) {
                // // 1. Send topic length (ensure network byte order)
                // uint32_t net_value = htonl(connection::MESSAGE_OP_TOPIC_PONG.size()); // Convert to network byte order
                // send(client_fd, &net_value, 4, 0);

                // // 2. Send topic string data
                // send(client_fd, connection::MESSAGE_OP_TOPIC_PONG.c_str(), connection::MESSAGE_OP_TOPIC_PONG.size(), 0);

                // string pong_message = "ok";
                // // 3. Send the string length (ensure network byte order)
                // uint32_t msg_len = htonl(pong_message.length()); // Send length of string
                // send(client_fd, &msg_len, 4, 0);

                // // 4. Send the string data
                // send(client_fd, pong_message.c_str(), pong_message.length(), 0);

                if (this->sendSocketData(client_fd, connection::MESSAGE_OP_TOPIC_PONG, "ok")) {
                    this->refreshKeepAlive(client_ip, client_port);
                }

            } else if (topic_buffer.data() == connection::MESSAGE_OP_TOPIC_PONG) {
                // Ingore this topic
            } else if (topic_buffer.data() == connection::MESSAGE_OP_TOPIC_SUBSCRIBE) {
                // Ingore this topic in publisher
            } else {
                string message_text = message_buffer.data();
                
                #ifdef OPEN_STD_DEBUG_LOG
                    std::cout << "recevie message json from " << client_ip << ":" << client_port << " " << topic_buffer.data() << "," << message_text << std::endl;
                #endif

                if (!context.is_allown_topic(topic_buffer.data())) {
                    warn_log("not allown topic: {}", topic_buffer.data());
                    continue;
                }

                if (message_text.size() > config.max_message_body_size) {
                    warn_log("too large message body: {}", message_text);
                    continue;
                }

                if (context.get_message_circle_composite()->createCircleIfAbsent(topic_buffer.data(), config.max_topic_circle_size)) {
                    context.get_message_circle_composite()->appendMessageToCircle(topic_buffer.data(), message_text);
                    if (config.subscriber_enable_event_loop) {
                        context.push_message_topic_for_event_loop(topic_buffer.data());
                    }
                } else {
                    warn_log("cannot create circle for {} {}", topic_buffer.data(), message_text);
                }
            }
        }
        close(client_fd);
        this->killAlive(client_ip, client_port);
    }

    void PublisherBootstrap::clearConnectionResource(repeater::GlobalContext &context, string client_ip, int client_port) {
        // nothing need to clear
    }
}