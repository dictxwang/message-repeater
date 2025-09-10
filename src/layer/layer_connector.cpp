#include "layer_connector.h"

#include "connection/acceptor.h"
#include "util/string_helper.h"

using namespace std;

namespace layer {

    void start_layer_replay(repeater::RepeaterConfig &config, repeater::GlobalContext &context) {

        if (!context.is_enable_layer_subscribe()) {
            info_log("not enable layer subscribe");
            return;
        }
        if (context.get_layer_subscribe_topics().size() == 0 || context.get_layer_subscribe_addresses().size() == 0) {
            warn_log("no support topic or address to layer subscribe");
            return;
        }

        // prepare messsage circle
        for (string topic : context.get_layer_subscribe_topics()) {
            bool result = context.get_message_circle_composite()->createCircleIfAbsent(topic, config.max_topic_circle_size);
            if (!result) {
                warn_log("fail to create circle for topic {}", topic);
            } else {
                warn_log("success to create circle for topic {}", topic);
            }
        }

        // start work thread for every address
        for (string address : context.get_layer_subscribe_addresses()) {
            thread work_thread(layer_replay_work, ref(config), ref(context), address);
            work_thread.detach();
            info_log("start layer replay word thread for address {}", address);
        }
    }

    void layer_replay_work(repeater::RepeaterConfig &config, repeater::GlobalContext &context, string subscribe_address) {

        vector<string> pair;
        strHelper::splitStr(pair, subscribe_address, ":");
        if (pair.size() != 2) {
            warn_log("invalid layer subscirbe address: {}", subscribe_address);
            return;
        }
        const string server_ip = pair[0];
        const int server_port = std::stoi(pair[1]);

        struct sockaddr_in server_address;
        memset(&server_address, 0, sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;
        server_address.sin_port = htons(server_port);

        // Convert IP address from string to binary form
        if (inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr) <= 0) {
            warn_log("invalid address or not suppoted: {}", subscribe_address);
            return;
        }

        while (true) {
            this_thread::sleep_for(chrono::seconds(5));

            // 1. Create client socket
            int client_fd = 0;
            if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                warn_log("fail to create socket for layer replay {}", subscribe_address);
                continue;
            }

            // 2. Connect to the server
            if (connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
                warn_log("fail to connect for layer replay {}", subscribe_address);
                close(client_fd);
                continue;
            }

            // 3. Send subscribe message
            Json::Value body_json;
            for (string topic : context.get_layer_subscribe_topics()) {
                body_json["topics"].append(topic);
            }
            string body = common_tools::serialize_json_value(body_json);
            info_log("layer replay subscribe message body: {}", body);

            if (!send_socket_data(client_fd, connection::MESSAGE_OP_TOPIC_SUBSCRIBE, body)) {
                warn_log("fail to send subscribe message to layer replay {}", subscribe_address);
                close(client_fd);
                continue;
            }

            // 4. Read subscribe response
            auto subscribe_resp = read_socket_frame(client_fd);
            if (!subscribe_resp.has_value()) {
                warn_log("fail to read subscribe response from layer replay {}", subscribe_address);
                close(client_fd);
                continue;
            }

            if (subscribe_resp.value().first != connection::MESSAGE_OP_TOPIC_SUBSCRIBE) {
                warn_log("not subscribe topic");
                close(client_fd);
                continue;
            }
            if (subscribe_resp.value().second.find("ok") == std::string::npos) {
                // fail
                warn_log("fail to subscribe layer replay: {}", subscribe_resp.value().second);
                close(client_fd);
                continue;
            } else {
                info_log("success to subscribe layer replay: {}", subscribe_address);
            }

            shared_ptr<bool> socket_disconnected = std::make_shared<bool>(false);
            // 5. Start ping thread
            thread ping_thread([client_fd, &subscribe_address, socket_disconnected] {
                
                while (true) {
                    bool is_disconnected = false;
                    for (int i = 0; i < 20; i++) {
                        this_thread::sleep_for(chrono::milliseconds(500));
                        if ((*socket_disconnected)) {
                            is_disconnected = true;
                            break;
                        }
                    }
                    if (is_disconnected) {
                        break;
                    }

                    // send ping
                    bool send_result = send_socket_data(client_fd, connection::MESSAGE_OP_TOPIC_PING, "ok");
                    if (!send_result) {
                        warn_log("fail to send ping to layer replay: {}", subscribe_address);
                        (*socket_disconnected) = true;
                        break;
                    }
                }
                info_log("ping thread exit for layer replay: {}", subscribe_address);
            });
            ping_thread.detach();

            uint64_t latest_ping_time = common_tools::get_current_epoch();
            // 6. Start reading loop
            while (true) {

                auto frame = read_socket_frame(client_fd);
                if (!frame.has_value()) {
                    warn_log("fail to read from layer replay: {}", subscribe_address);
                    break;
                }

                string topic_name = frame.value().first;
                string message_body = frame.value().second;
                if (topic_name == connection::MESSAGE_OP_TOPIC_PONG) {
                    info_log("receive pong from layer replay: {}", subscribe_address);
                    continue;
                } else if (context.is_allown_topic(topic_name)) {
                    bool appended = context.get_message_circle_composite()->appendMessageToCircle(topic_name, message_body);
                    if (!appended) {
                        warn_log("fail to append message for layer replay {} {}", topic_name, message_body);
                    }
                    #ifdef OPEN_STD_DEBUG_LOG
                        std::cout << "append message for layer replay " << topic_name << "," << message_body << "," << appended << std::endl;
                    #endif
                } else {
                    warn_log("not supported topic from layer replay: {} {}", topic_name, subscribe_address);
                    continue;
                }
            }

            (*socket_disconnected) = true;
            close(client_fd);
        }
    }

    bool send_socket_data(int client_fd, string topic, string message) {

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
            #ifdef OPEN_STD_DEBUG_LOG
                std::cout << "exception sending data occurred: " << e.what() << std::endl;
            #endif
            return false;
        }
        
        if (bytes_sent < 0) {
            #ifdef OPEN_STD_DEBUG_LOG
                std::cout << "error sending data or timeout occurred" << std::endl;
            #endif
            return false;
        } else if (bytes_sent == 0) {
            #ifdef OPEN_STD_DEBUG_LOG
                std::cout << "no data sent (possibly due to timeout or closed connection)" << std::endl;
            #endif
            return false;
        } else {
            #ifdef OPEN_STD_DEBUG_LOG
                std::cout << "server sent " << bytes_sent << " bytes" << std::endl;
            #endif
        }
        return true;
    }

    optional<pair<string, string>> read_socket_frame(int client_fd) {

        size_t HEADER_SIZE = 4;
        size_t MAX_MESSAGE_SIZE = 65536;

        // Step1: read header of topic length
        uint32_t topic_length = 0;
        ssize_t bytes_received = recv(client_fd, &topic_length, HEADER_SIZE, MSG_WAITALL);
        if (bytes_received != HEADER_SIZE) {
            if (bytes_received == 0) {
                err_log("client of layer replay disconnected");
            } else {
                err_log("fail to read header of type from client of layer replay");
            }
            return nullopt;
        }
        topic_length = ntohl(topic_length);

        // Step2: read header of topic name
        std::vector<char> topic_buffer(topic_length + 1);
        bytes_received = recv(client_fd, topic_buffer.data(), topic_length, MSG_WAITALL);
        if (bytes_received != topic_length) {
            err_log("fail to read complete topic from client of layer replay");
            return nullopt;
        }

        // Step3: read header of main data length
        uint32_t message_length = 0;
        bytes_received = recv(client_fd, &message_length, HEADER_SIZE, MSG_WAITALL);
        if (bytes_received != HEADER_SIZE) {
            if (bytes_received == 0) {
                err_log("client of layer replay disconnected");
            } else {
                err_log("fail to read header of length from client of layer replay");
            }
            return nullopt;
        }
        message_length = ntohl(message_length);

        if (message_length > MAX_MESSAGE_SIZE) {
            err_log("message from client of layer replay is too large, which length is {}", message_length);
            return nullopt;
        }

        // Step4: read main data
        std::vector<char> message_buffer(message_length + 1);
        bytes_received = recv(client_fd, message_buffer.data(), message_length, MSG_WAITALL);
        
        if (bytes_received != message_length) {
            err_log("fail to read complete message from client of layer replay");
            return nullopt;
        }

        message_buffer[message_length] = '\0';
        
        return std::make_pair<string, string>(std::string(topic_buffer.data()), std::string(message_buffer.data()));
    }
}
