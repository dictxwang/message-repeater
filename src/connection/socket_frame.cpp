#include "socket_frame.h"

#include <cerrno>
#include <cstdint>
#include <sys/socket.h>
#include <arpa/inet.h>

namespace connection {

    std::vector<char> encodeSocketFrame(const std::string &topic, const std::string &message) {
        std::vector<char> buffer;
        buffer.reserve(sizeof(uint32_t) * 2 + topic.size() + message.size());

        uint32_t topic_length = htonl(static_cast<uint32_t>(topic.size()));
        buffer.insert(
            buffer.end(),
            reinterpret_cast<const char*>(&topic_length),
            reinterpret_cast<const char*>(&topic_length) + sizeof(topic_length));
        buffer.insert(buffer.end(), topic.begin(), topic.end());

        uint32_t message_length = htonl(static_cast<uint32_t>(message.size()));
        buffer.insert(
            buffer.end(),
            reinterpret_cast<const char*>(&message_length),
            reinterpret_cast<const char*>(&message_length) + sizeof(message_length));
        buffer.insert(buffer.end(), message.begin(), message.end());
        return buffer;
    }

    NonBlockingSendStatus sendSocketFrameNonBlocking(
        int client_fd,
        const std::vector<char> &frame,
        std::size_t &offset) {

        while (offset < frame.size()) {
            ssize_t bytes_sent = send(
                client_fd,
                frame.data() + offset,
                frame.size() - offset,
                MSG_NOSIGNAL | MSG_DONTWAIT);
            if (bytes_sent > 0) {
                offset += static_cast<std::size_t>(bytes_sent);
                continue;
            }
            if (bytes_sent < 0 && errno == EINTR) {
                continue;
            }
            if (bytes_sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return NonBlockingSendStatus::WouldBlock;
            }
            return NonBlockingSendStatus::Error;
        }
        return NonBlockingSendStatus::Complete;
    }

}
