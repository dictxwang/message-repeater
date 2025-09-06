#include "acceptor.h"

namespace connection {

    void AbstractBootstrap::init(string role, string listen_address, int listen_port, int max_connection) {
        this->listen_address_ = listen_address;
        this->listen_port_ = listen_port;
        this->max_connection_ = max_connection;
        this->role_ = role;

        // Create socket
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) {
            err_log("fail to create socket: {}", strerror(errno));
            return;
        }
        
        // Allow socket reuse
        int opt = 1;
        if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
            err_log("fail to set socket options: {}", strerror(errno));
            close(server_fd_);
            return;
        }
        
        // Bind address
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_port = htons(listen_port);
        
        if (inet_pton(AF_INET, listen_address.c_str(), &address.sin_addr) <= 0) {
            err_log("invalid address: {}", listen_address);
            close(server_fd_);
            return;
        }

        bind(server_fd_, (struct sockaddr*)&address, sizeof(address));
        
        // Start listening
        if (listen(server_fd_, max_connection) < 0) {
            err_log("failed to listen: {}", strerror(errno));
            close(server_fd_);
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


        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        if (this->client_connections_.size() >= this->max_connection_) {
            warn_log("{} server has max connection", this->role_);
            return;
        }

        if (this->server_fd_ < 0) {
            err_log("server not initailized");
            return;
        }
        
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        while (true) {
            int client_fd = ::accept(this->server_fd_, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd < 0) {
                if (errno == EINTR) {
                    continue;  // Interrupted, retry
                }
                err_log("failed to accept connection: {}", strerror(errno));
                continue;
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

            info_log("{} create new connection from {}:{} with fd={}", client_ip, client_port, client_fd);
            
            // Here you would typically:
            // 1. Set socket options (non-blocking, keepalive, etc)
            // 2. Create a connection handler
            // 3. Pass to thread pool or event loop
            
            // // For now, just echo a message and close
            // const char* welcome = "Connected to TCP server\n";
            // send(client_fd, welcome, strlen(welcome), 0);
            
            // // In real implementation, keep connection open and handle it
            // close(client_fd);

            thread handle_thread(acceptHandle, ref(config), ref(context), client_fd);
            handle_thread.detach();
        }
    }

    void AbstractBootstrap::startAliveDetection(repeater::RepeaterConfig &config, repeater::GlobalContext context) {

        // TODO
    }
}