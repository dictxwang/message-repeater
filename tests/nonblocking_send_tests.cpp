#include "connection/socket_frame.h"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
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

    void closeSocketPair(int sockets[2]) {
        close(sockets[0]);
        close(sockets[1]);
    }

    bool testNonBlockingSend() {
        const std::string topic = "Sample0001";
        const std::string message(2 * 1024 * 1024, 'x');
        const std::vector<char> frame = connection::encodeSocketFrame(topic, message);

        uint32_t encoded_topic_length = 0;
        std::memcpy(&encoded_topic_length, frame.data(), sizeof(encoded_topic_length));
        if (ntohl(encoded_topic_length) != topic.size()) {
            std::cerr << "topic length was encoded incorrectly\n";
            return false;
        }

        int sockets[2] = {-1, -1};
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
            std::cerr << "failed to create socket pair\n";
            return false;
        }

        int send_buffer_bytes = 4096;
        if (setsockopt(
                sockets[0], SOL_SOCKET, SO_SNDBUF,
                &send_buffer_bytes, sizeof(send_buffer_bytes)) != 0) {
            std::cerr << "failed to set test send buffer\n";
            closeSocketPair(sockets);
            return false;
        }
        int socket_flags = fcntl(sockets[0], F_GETFL, 0);
        if (socket_flags < 0 || fcntl(sockets[0], F_SETFL, socket_flags | O_NONBLOCK) != 0) {
            std::cerr << "failed to set non-blocking test socket\n";
            closeSocketPair(sockets);
            return false;
        }

        std::size_t offset = 0;
        bool observed_would_block = false;
        std::vector<char> received;
        for (int attempt = 0; attempt < 10000 && offset < frame.size(); ++attempt) {
            connection::NonBlockingSendStatus status =
                connection::sendSocketFrameNonBlocking(sockets[0], frame, offset);
            if (status == connection::NonBlockingSendStatus::Error) {
                std::cerr << "non-blocking send failed\n";
                closeSocketPair(sockets);
                return false;
            }
            if (status == connection::NonBlockingSendStatus::WouldBlock) {
                observed_would_block = true;
                if (!drainSocket(sockets[1], received)) {
                    std::cerr << "failed to drain test socket\n";
                    closeSocketPair(sockets);
                    return false;
                }
            }
        }

        shutdown(sockets[0], SHUT_WR);
        if (!drainSocket(sockets[1], received)) {
            std::cerr << "failed to drain final test bytes\n";
            closeSocketPair(sockets);
            return false;
        }

        closeSocketPair(sockets);
        if (!observed_would_block) {
            std::cerr << "test did not exercise a full send buffer\n";
            return false;
        }
        if (offset != frame.size() || received != frame) {
            std::cerr << "partial frame resume corrupted the frame\n";
            return false;
        }
        return true;
    }

    bool testBlockingFrameRoundTrip() {
        int sockets[2] = {-1, -1};
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
            std::cerr << "failed to create blocking socket pair\n";
            return false;
        }

        int send_buffer_bytes = 4096;
        timeval send_timeout{2, 0};
        if (setsockopt(
                sockets[0], SOL_SOCKET, SO_SNDBUF,
                &send_buffer_bytes, sizeof(send_buffer_bytes)) != 0 ||
            setsockopt(
                sockets[0], SOL_SOCKET, SO_SNDTIMEO,
                &send_timeout, sizeof(send_timeout)) != 0) {
            std::cerr << "failed to configure blocking test socket\n";
            closeSocketPair(sockets);
            return false;
        }

        const std::string topic = "layer-topic";
        const std::string message(512 * 1024, 'b');
        const std::vector<char> frame = connection::encodeSocketFrame(topic, message);
        connection::SocketFrameReadResult read_result{
            connection::SocketFrameReadStatus::Error, {}, {}};
        std::thread reader([&] {
            read_result = connection::readSocketFrameBlocking(
                sockets[1], 1024, 1024 * 1024);
        });

        const bool sent = connection::sendSocketFrameBlocking(sockets[0], frame);
        reader.join();
        closeSocketPair(sockets);

        if (!sent || read_result.status != connection::SocketFrameReadStatus::Complete ||
            read_result.topic != topic || read_result.message != message) {
            std::cerr << "blocking frame round trip failed\n";
            return false;
        }
        return true;
    }

    bool testReadTimeout() {
        int sockets[2] = {-1, -1};
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
            std::cerr << "failed to create timeout socket pair\n";
            return false;
        }

        timeval read_timeout{0, 200000};
        if (setsockopt(
                sockets[0], SOL_SOCKET, SO_RCVTIMEO,
                &read_timeout, sizeof(read_timeout)) != 0) {
            std::cerr << "failed to configure read timeout\n";
            closeSocketPair(sockets);
            return false;
        }

        const auto started = std::chrono::steady_clock::now();
        connection::SocketFrameReadResult result = connection::readSocketFrameBlocking(
            sockets[0], 1024, 1024);
        const auto elapsed = std::chrono::steady_clock::now() - started;
        closeSocketPair(sockets);

        if (result.status != connection::SocketFrameReadStatus::Timeout ||
            elapsed > std::chrono::seconds(2)) {
            std::cerr << "socket read timeout was not reported\n";
            return false;
        }
        return true;
    }

    bool testShutdownWakesReader() {
        int sockets[2] = {-1, -1};
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
            std::cerr << "failed to create shutdown socket pair\n";
            return false;
        }

        timeval read_timeout{5, 0};
        if (setsockopt(
                sockets[0], SOL_SOCKET, SO_RCVTIMEO,
                &read_timeout, sizeof(read_timeout)) != 0) {
            std::cerr << "failed to configure shutdown test timeout\n";
            closeSocketPair(sockets);
            return false;
        }

        connection::SocketFrameReadResult read_result{
            connection::SocketFrameReadStatus::Error, {}, {}};
        const auto started = std::chrono::steady_clock::now();
        std::thread reader([&] {
            read_result = connection::readSocketFrameBlocking(
                sockets[0], 1024, 1024);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        shutdown(sockets[0], SHUT_RDWR);
        reader.join();
        const auto elapsed = std::chrono::steady_clock::now() - started;
        closeSocketPair(sockets);

        if (read_result.status != connection::SocketFrameReadStatus::Closed ||
            elapsed > std::chrono::seconds(2)) {
            std::cerr << "shutdown did not wake the socket reader\n";
            return false;
        }
        return true;
    }

}

int main() {
    if (!testNonBlockingSend() ||
        !testBlockingFrameRoundTrip() ||
        !testReadTimeout() ||
        !testShutdownWakesReader()) {
        return 1;
    }

    std::cout << "all socket frame tests passed\n";
    return 0;
}
