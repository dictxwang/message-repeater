#include "layer_connector.h"

#include "connection/socket_frame.h"
#include "connection/acceptor.h"
#include "util/string_helper.h"

#include <algorithm>
#include <atomic>

using namespace std;

namespace layer {

    namespace {

        constexpr int LAYER_PING_INTERVAL_SECONDS = 10;
        constexpr int LAYER_MIN_READ_TIMEOUT_SECONDS = LAYER_PING_INTERVAL_SECONDS * 2;
        constexpr size_t MAX_LAYER_FRAME_FIELD_SIZE = 65536;

        bool set_socket_timeout(int client_fd, int option, int timeout_seconds) {
            timeval timeout{};
            timeout.tv_sec = timeout_seconds;
            return setsockopt(
                client_fd,
                SOL_SOCKET,
                option,
                &timeout,
                sizeof(timeout)) == 0;
        }

    }

    void start_layer_replay(repeater::RepeaterConfig &config, repeater::GlobalContext &context) {

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
            info_log("start layer replay work thread for address {}", address);
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

            const int read_timeout_seconds = std::max(
                config.max_connection_idle_second,
                LAYER_MIN_READ_TIMEOUT_SECONDS);
            const int write_timeout_seconds = std::max(
                config.socket_write_timeout_second,
                1);
            if (!set_socket_timeout(client_fd, SO_RCVTIMEO, read_timeout_seconds)) {
                warn_log("fail to set read timeout for layer replay {}", subscribe_address);
                close(client_fd);
                continue;
            }
            if (!set_socket_timeout(client_fd, SO_SNDTIMEO, write_timeout_seconds)) {
                warn_log("fail to set write timeout for layer replay {}", subscribe_address);
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

            atomic_bool socket_disconnected{false};
            // 5. Start ping thread
            thread ping_thread([client_fd, subscribe_address, &socket_disconnected] {
                
                while (true) {
                    bool is_disconnected = false;
                    for (int i = 0; i < LAYER_PING_INTERVAL_SECONDS * 2; i++) {
                        this_thread::sleep_for(chrono::milliseconds(500));
                        if (socket_disconnected.load()) {
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
                        socket_disconnected.store(true);
                        shutdown(client_fd, SHUT_RDWR);
                        break;
                    }
                }
                info_log("ping thread exit for layer replay: {}", subscribe_address);
            });

            // 6. Start reading loop
            while (!socket_disconnected.load()) {

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
                    } else {
                        if (config.subscriber_enable_event_loop) {
                            // context.push_message_topic_for_event_loop(topic_name);
                            bool queued = context.submit_message_topic_to_event_loop(topic_name);
                            if (queued) {
                                bool notifyResult = context.notify_message_topic_to_event_loop();
                                if (!notifyResult) {
                                    warn_log("fail to notify event loop to start for layer replay {} {}", topic_name, message_body);
                                }
                            }
                        }
                    }
                    #ifdef OPEN_STD_DEBUG_LOG
                        std::cout << "append message for layer replay " << topic_name << "," << message_body << "," << appended << std::endl;
                    #endif
                } else {
                    warn_log("not supported topic from layer replay: {} {}", topic_name, subscribe_address);
                    continue;
                }
            }

            socket_disconnected.store(true);
            shutdown(client_fd, SHUT_RDWR);
            if (ping_thread.joinable()) {
                ping_thread.join();
            }
            close(client_fd);
        }
    }

    bool send_socket_data(int client_fd, string topic, string message) {
        const vector<char> frame = connection::encodeSocketFrame(topic, message);
        return connection::sendSocketFrameBlocking(client_fd, frame);
    }

    optional<pair<string, string>> read_socket_frame(int client_fd) {
        connection::SocketFrameReadResult result = connection::readSocketFrameBlocking(
            client_fd,
            MAX_LAYER_FRAME_FIELD_SIZE,
            MAX_LAYER_FRAME_FIELD_SIZE);
        switch (result.status) {
            case connection::SocketFrameReadStatus::Complete:
                return std::make_pair(std::move(result.topic), std::move(result.message));
            case connection::SocketFrameReadStatus::Timeout:
                warn_log("read timeout from layer replay");
                break;
            case connection::SocketFrameReadStatus::Closed:
                warn_log("layer replay connection was closed");
                break;
            case connection::SocketFrameReadStatus::TooLarge:
                err_log("frame from layer replay is too large");
                break;
            case connection::SocketFrameReadStatus::Error:
                err_log("fail to read frame from layer replay");
                break;
        }
        return nullopt;
    }
}
