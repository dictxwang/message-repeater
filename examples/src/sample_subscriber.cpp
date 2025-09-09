#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdint>
#include <unordered_map>
#include <thread>
#include <iostream>
#include <cstring>
#include "util/common_tool.h"
#include "util/string_helper.h"
#include "json/json.h"

using namespace std;

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
        return false;
    }
    
    if (bytes_sent < 0) {
        return false;
    } else if (bytes_sent == 0) {
        return false;
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
            std::cout << "client of layer replay disconnected" << std::endl;
        } else {
            std::cout << "fail to read header of type from client of layer replay" << std::endl;
        }
        return nullopt;
    }
    topic_length = ntohl(topic_length);

    // Step2: read header of topic name
    std::vector<char> topic_buffer(topic_length + 1);
    bytes_received = recv(client_fd, topic_buffer.data(), topic_length, MSG_WAITALL);
    if (bytes_received != topic_length) {
        std::cout << "fail to read complete topic from client of layer replay" << std::endl;
        return nullopt;
    }

    // Step3: read header of main data length
    uint32_t message_length = 0;
    bytes_received = recv(client_fd, &message_length, HEADER_SIZE, MSG_WAITALL);
    if (bytes_received != HEADER_SIZE) {
        if (bytes_received == 0) {
            std::cout << "client of layer replay disconnected" << std::endl;
        } else {
            std::cout << "fail to read header of length from client of layer replay" << std::endl;
        }
        return nullopt;
    }
    message_length = ntohl(message_length);

    if (message_length > MAX_MESSAGE_SIZE) {
        std::cout << "message from client of layer replay is too large, which length is " << message_length << std::endl;
        return nullopt;
    }

    // Step4: read main data
    std::vector<char> message_buffer(message_length + 1);
    bytes_received = recv(client_fd, message_buffer.data(), message_length, MSG_WAITALL);
    
    if (bytes_received != message_length) {
        std::cout << "fail to read complete message from client of layer replay" << std::endl;
        return nullopt;
    }

    message_buffer[message_length] = '\0';
    
    return std::make_pair<string, string>(std::string(topic_buffer.data()), std::string(message_buffer.data()));
}

int main(int argc, char const *argv[]) {
    
    vector<string> topics;
    topics.push_back("T001");
    topics.push_back("T002");

    string server_ip = "127.0.0.1";
    int server_port = 20001;

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(server_port);

    // Convert IP address from string to binary form
    if (inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr) <= 0) {
        std::cout << "invalid address or not suppoted: " << server_ip << std::endl;
        return -1;
    }

    while (true) {
        this_thread::sleep_for(chrono::seconds(5));

        // 1. Create client socket
        int client_fd = 0;
        if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            std::cout << "fail to create socket for layer replay" << std::endl;
            continue;
        }

        // 2. Connect to the server
        if (connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
            std::cout << "fail to connect for layer replay" << std::endl;
            close(client_fd);
            continue;
        }

        // 3. Send subscribe message
        Json::Value body_json;
        for (string topic : topics) {
            body_json["topics"].append(topic);
        }
        string body = common_tools::serialize_json_value(body_json);
        std::cout << "layer replay subscribe message body: " << body << std::endl;

        if (!send_socket_data(client_fd, "subscribe", body)) {
            std::cout << "fail to send subscribe message to layer replay" << std::endl;
            close(client_fd);
            continue;
        }

        // 4. Read subscribe response
        auto subscribe_resp = read_socket_frame(client_fd);
        if (!subscribe_resp.has_value()) {
            std::cout << "fail to read subscribe response from layer replay" << std::endl;
            close(client_fd);
            continue;
        }

        if (subscribe_resp.value().first != "subscribe") {
            std::cout << "not subscribe topic" << std::endl;
            close(client_fd);
            continue;
        }
        if (subscribe_resp.value().second.find("ok") == std::string::npos) {
            // fail
            std::cout << "fail to subscribe layer replay: " << subscribe_resp.value().second << std::endl;
            close(client_fd);
            continue;
        } else {
            std::cout << "success to subscribe layer replay" << std::endl;
        }

        bool socket_disconnected = false;
        // 5. Start ping thread
        thread ping_thread([client_fd, &socket_disconnected] {
            bool is_disconnected = false;

            while (true) {
                for (int i = 0; i < 20; i++) {
                    this_thread::sleep_for(chrono::milliseconds(500));
                    if (socket_disconnected) {
                        is_disconnected = true;
                        break;
                    }
                }
                if (is_disconnected) {
                    break;
                }

                // send ping
                bool send_result = send_socket_data(client_fd, "ping", "ok");
                if (!send_result) {
                    std::cout << "fail to send ping to layer replay" << std::endl;
                    socket_disconnected = true;
                    break;
                }
            }
            std::cout << "ping thread exit for layer replay" << std::endl;
        });
        ping_thread.detach();

        uint64_t latest_ping_time = common_tools::get_current_epoch();
        // 6. Start reading loop
        while (true) {

            auto frame = read_socket_frame(client_fd);
            if (!frame.has_value()) {
                std::cout << "fail to read from layer replay" << std::endl;
                break;
            }

            string topic_name = frame.value().first;
            string message_body = frame.value().second;
            if (topic_name == "pong") {
                std::cout << "receive pong message: " << std::endl;
                continue;
            } else {
                std::cout << "receive normal message: " << topic_name << "," << message_body << std::endl;
            }
        }

        socket_disconnected = true;
        close(client_fd);
    }
    
    while(true) {
        std::cout << "subscriber starter keep running" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    return 0;
}
