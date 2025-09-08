#include "acceptor.h"

namespace connection {

    void AbstractBootstrap::init(string role, string listen_address, int listen_port, int max_connection) {
        this->listen_address_ = listen_address;
        this->listen_port_ = listen_port;
        this->max_connection_ = max_connection;
        this->role_ = role;

        // Create socket
        this->server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (this->server_fd_ <= 0) {
            err_log("fail to create socket: {}", strerror(errno));
            return;
        }

        // Allow socket reuse
        int opt = 1;
        if (setsockopt(this->server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
            err_log("fail to set socket options: {}", strerror(errno));
            close(this->server_fd_);
            return;
        }

        // Bind address
        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(listen_port);

        if (inet_pton(AF_INET, listen_address.c_str(), &address.sin_addr) <= 0) {
            err_log("invalid address: {}", listen_address);
            close(this->server_fd_);
            return;
        }

        if (::bind(this->server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
            err_log("failed to bind: {}", strerror(errno));
            close(this->server_fd_);
            return;
        }

        // Start listening
        if (listen(this->server_fd_, 10) < 0) {
            err_log("failed to listen: {}", strerror(errno));
            close(this->server_fd_);
            return;
        }

        info_log("{} listening at {}:{}", this->role_, this->listen_address_, this->listen_port_);
    }

    void AbstractBootstrap::start(repeater::RepeaterConfig &config, repeater::GlobalContext &context) {

        thread accept_thread([&] {
            startAccept(config, context);
        });
        accept_thread.detach();
        info_log("{} start accept thread", this->role_);

        thread detection_thread([&] {
            startAliveDetection(config, context);
        });
        detection_thread.detach();
        info_log("{} start detection thread", this->role_);
    }

    void AbstractBootstrap::startAccept(repeater::RepeaterConfig &config, repeater::GlobalContext &context) {

        if (this->server_fd_ < 0) {
            err_log("server not initailized");
            return;
        }
        
        while (true) {

            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            int client_fd = accept(this->server_fd_, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd < 0) {
                if (errno == EINTR) {
                    continue;  // Interrupted, retry
                }
                err_log("failed to accept connection: {}", strerror(errno));
                continue;
            }

            std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
            if (this->client_connections_.size() >= this->max_connection_) {
                warn_log("{} server reach max connection", this->role_);
                // TODO send response
                close(client_fd);
                return;
            }

            // Set Write timeout
            struct timeval timeout;
            timeout.tv_sec = config.socket_write_timeout_second;  // 5 seconds
            timeout.tv_usec = 0; // 0 microseconds
            if (setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
                warn_log("{} fail to set write timeout for client", this->role_);
                close(client_fd);
                return;
            }

            // Get client info
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(client_addr.sin_port);

            ConnectionEntity entity;
            entity.clientIP = client_ip;
            entity.clientPort = client_port;
            entity.clientFd = client_fd;
            entity.latestHeartbeat = common_tools::get_current_epoch();

            string key = std::string(client_ip) + ":" + std::to_string(client_port);
            this->client_connections_[key] = entity;

            info_log("{} create new connection from {}:{} with client_fd={}", this->role_, client_ip, client_port, client_fd);
            
            // Here you would typically:
            // 1. Set socket options (non-blocking, keepalive, etc)
            // 2. Create a connection handler
            // 3. Pass to thread pool or event loop
            
            // // For now, just echo a message and close
            // const char* welcome = "Connected to TCP server\n";
            // send(client_fd, welcome, strlen(welcome), 0);
            
            // // In real implementation, keep connection open and handle it
            // close(client_fd);

            thread handle_thread(&AbstractBootstrap::acceptHandle, this, ref(config), ref(context), client_fd, std::string(client_ip), client_port);
            handle_thread.detach();
        }
    }

    void AbstractBootstrap::startAliveDetection(repeater::RepeaterConfig &config, repeater::GlobalContext context) {

        while (true) {
            this_thread::sleep_for(chrono::seconds(1));
            std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);

            uint64_t now = common_tools::get_current_epoch();
            vector<string> idle_expired_keys;
            for (auto [k, v] : this->client_connections_) {
                if (v.latestHeartbeat + config.max_connection_idle_second < now) {
                    idle_expired_keys.push_back(k);
                    close(v.clientFd);
                    info_log("{} close connection {}:{}", this->role_, v.clientIP, v.clientPort);
                }
            }
            for (string key : idle_expired_keys) {
                this->client_connections_.erase(key);
                info_log("{} remove connection {}", this->role_, key);
            }
            // TODO remove circle
            w_lock.unlock();
        }
    }

    void AbstractBootstrap::refreshKeepAlive(string client_ip, int client_port) {

        string key = std::string(client_ip) + ":" + std::to_string(client_port);
        auto connection = this->client_connections_.find(key);
        if (connection != this->client_connections_.end()) {
            connection->second.latestHeartbeat = common_tools::get_current_epoch();
            std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
            this->client_connections_[key] = connection->second;
        }
    }

    void AbstractBootstrap::acceptHandle(repeater::RepeaterConfig &config, repeater::GlobalContext &context, int client_fd, string client_ip, int client_port) {
        // Base implementation - derived classes should override
        close(client_fd);
    }

    bool AbstractBootstrap::sendSocketData(int client_fd, string topic, string message) {
        
        // // 1. Send topic length (ensure network byte order)
        // uint32_t net_value = htonl(topic.size()); // Convert to network byte order
        // ssize_t bytes_sent = send(client_fd, &net_value, 4, 0);
        // if (bytes_sent)

        // // 2. Send topic string data
        // send(client_fd, topic.c_str(), topic.size(), 0);

        // // 3. Send the string length (ensure network byte order)
        // uint32_t msg_len = htonl(message.length()); // Send length of string
        // send(client_fd, &msg_len, 4, 0);

        // // 4. Send the string data
        // send(client_fd, message.c_str(), message.length(), 0);

        vector<char> buffer;
        // Serialize string length
        size_t topic_length = topic.length();
        buffer.insert(buffer.end(), reinterpret_cast<const char*>(&topic_length),
                    reinterpret_cast<const char*>(&topic_length) + sizeof(topic_length));
        // Serialize string data
        buffer.insert(buffer.end(), topic.begin(), topic.end());

        // Serialize string length
        size_t message_length = message.length();
        buffer.insert(buffer.end(), reinterpret_cast<const char*>(&message_length),
                    reinterpret_cast<const char*>(&message_length) + sizeof(message_length));
        // Serialize string data
        buffer.insert(buffer.end(), message.begin(), message.end());

        ssize_t bytes_sent = send(client_fd, buffer.data(), buffer.size(), 0);
        
        if (bytes_sent < 0) {
            #ifdef OPEN_STD_DEBUG_LOG
                std::cout << "Error sending data or timeout occurred" << std::endl;
            #endif
            return false;
        } else if (bytes_sent == 0) {
            #ifdef OPEN_STD_DEBUG_LOG
                std::cout << "No data sent (possibly due to timeout or closed connection)" << std::endl;
            #endif
            return false;
        } else {
            #ifdef OPEN_STD_DEBUG_LOG
                std::cout << "Sent " << bytes_sent << " bytes" << std::endl;
            #endif
        }
        return true;
    }
}