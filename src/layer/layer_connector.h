#ifndef _LAYER_CONNECTOR_H_
#define _LAYER_CONNECTOR_H_

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
#include <optional>
#include "logger/logger.h"
#include "config/repeater_config.h"
#include "combiner/global_context.h"
#include "util/common_tool.h"
#include "json/json.h"

using namespace std;

namespace layer {

    bool send_socket_data(int client_fd, string topic, string message);
    optional<pair<string, string>> read_socket_frame(int client_fd);
    void start_layer_replay(repeater::RepeaterConfig &config, repeater::GlobalContext &context);
    void layer_replay_work(repeater::RepeaterConfig &config, repeater::GlobalContext &context, string subscribe_address);
}

#endif