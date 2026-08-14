#include "connection/socket_frame.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

    bool drainSocket(int fd, std::vector<char> &received) {
        char buffer[8192];
        while (true) {
            ssize_t count = recv(fd, buffer, sizeof(buffer), MSG_DONTWAIT);
            if (count > 0) {
                received.insert(received.end(), buffer, buffer + count);
                continue;
            }
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return true;
            }
            return count == 0;
        }
    }

}

int main() {
    const std::string topic = "Sample0001";
    const std::string message(2 * 1024 * 1024, 'x');
    const std::vector<char> frame = connection::encodeSocketFrame(topic, message);

    uint32_t encoded_topic_length = 0;
    std::memcpy(&encoded_topic_length, frame.data(), sizeof(encoded_topic_length));
    if (ntohl(encoded_topic_length) != topic.size()) {
        std::cerr << "topic length was encoded incorrectly\n";
        return 1;
    }

    int sockets[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
        std::cerr << "failed to create socket pair\n";
        return 1;
    }

    int send_buffer_bytes = 4096;
    if (setsockopt(
            sockets[0], SOL_SOCKET, SO_SNDBUF,
            &send_buffer_bytes, sizeof(send_buffer_bytes)) != 0) {
        std::cerr << "failed to set test send buffer\n";
        close(sockets[0]);
        close(sockets[1]);
        return 1;
    }
    int socket_flags = fcntl(sockets[0], F_GETFL, 0);
    if (socket_flags < 0 || fcntl(sockets[0], F_SETFL, socket_flags | O_NONBLOCK) != 0) {
        std::cerr << "failed to set non-blocking test socket\n";
        close(sockets[0]);
        close(sockets[1]);
        return 1;
    }

    std::size_t offset = 0;
    bool observed_would_block = false;
    std::vector<char> received;
    for (int attempt = 0; attempt < 10000 && offset < frame.size(); ++attempt) {
        connection::NonBlockingSendStatus status =
            connection::sendSocketFrameNonBlocking(sockets[0], frame, offset);
        if (status == connection::NonBlockingSendStatus::Error) {
            std::cerr << "non-blocking send failed\n";
            close(sockets[0]);
            close(sockets[1]);
            return 1;
        }
        if (status == connection::NonBlockingSendStatus::WouldBlock) {
            observed_would_block = true;
            if (!drainSocket(sockets[1], received)) {
                std::cerr << "failed to drain test socket\n";
                close(sockets[0]);
                close(sockets[1]);
                return 1;
            }
        }
    }

    shutdown(sockets[0], SHUT_WR);
    if (!drainSocket(sockets[1], received)) {
        std::cerr << "failed to drain final test bytes\n";
        close(sockets[0]);
        close(sockets[1]);
        return 1;
    }

    close(sockets[0]);
    close(sockets[1]);

    if (!observed_would_block) {
        std::cerr << "test did not exercise a full send buffer\n";
        return 1;
    }
    if (offset != frame.size() || received != frame) {
        std::cerr << "partial frame resume corrupted the frame\n";
        return 1;
    }

    std::cout << "all non-blocking send tests passed\n";
    return 0;
}
