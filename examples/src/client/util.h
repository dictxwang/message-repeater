#ifndef _REPEATER_UTIL_H_
#define _REPEATER_UTIL_H_

#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdint>
#include <optional>

using namespace std;

namespace repeater_client {
    bool send_socket_data(const int client_fd, const string &topic, const string &message);
    optional<pair<string, string>> read_socket_frame(const int client_fd);
}
#endif