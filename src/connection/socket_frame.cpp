#include "socket_frame.h"

#include <cerrno>
#include <cstdint>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <utility>

namespace connection {

    namespace {

        SocketFrameReadStatus receiveExact(int client_fd, void *buffer, std::size_t size) {
            std::size_t offset = 0;
            char *output = static_cast<char *>(buffer);
            while (offset < size) {
                ssize_t bytes_received = recv(client_fd, output + offset, size - offset, 0);
                if (bytes_received > 0) {
                    offset += static_cast<std::size_t>(bytes_received);
                    continue;
                }
                if (bytes_received == 0) {
                    return SocketFrameReadStatus::Closed;
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return SocketFrameReadStatus::Timeout;
                }
                return SocketFrameReadStatus::Error;
            }
            return SocketFrameReadStatus::Complete;
        }

    }

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

    bool sendSocketFrameBlocking(int client_fd, const std::vector<char> &frame) {
        std::size_t offset = 0;
        while (offset < frame.size()) {
            ssize_t bytes_sent = send(
                client_fd,
                frame.data() + offset,
                frame.size() - offset,
                MSG_NOSIGNAL);
            if (bytes_sent > 0) {
                offset += static_cast<std::size_t>(bytes_sent);
                continue;
            }
            if (bytes_sent < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }
        return true;
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

    SocketFrameReadResult readSocketFrameBlocking(
        int client_fd,
        std::size_t max_topic_size,
        std::size_t max_message_size) {

        uint32_t encoded_topic_length = 0;
        SocketFrameReadStatus status = receiveExact(
            client_fd, &encoded_topic_length, sizeof(encoded_topic_length));
        if (status != SocketFrameReadStatus::Complete) {
            return {status, {}, {}};
        }

        const std::size_t topic_length = ntohl(encoded_topic_length);
        if (topic_length > max_topic_size) {
            return {SocketFrameReadStatus::TooLarge, {}, {}};
        }

        std::string topic(topic_length, '\0');
        status = receiveExact(client_fd, topic.data(), topic.size());
        if (status != SocketFrameReadStatus::Complete) {
            return {status, {}, {}};
        }

        uint32_t encoded_message_length = 0;
        status = receiveExact(
            client_fd, &encoded_message_length, sizeof(encoded_message_length));
        if (status != SocketFrameReadStatus::Complete) {
            return {status, {}, {}};
        }

        const std::size_t message_length = ntohl(encoded_message_length);
        if (message_length > max_message_size) {
            return {SocketFrameReadStatus::TooLarge, {}, {}};
        }

        std::string message(message_length, '\0');
        status = receiveExact(client_fd, message.data(), message.size());
        if (status != SocketFrameReadStatus::Complete) {
            return {status, {}, {}};
        }

        return {
            SocketFrameReadStatus::Complete,
            std::move(topic),
            std::move(message)};
    }

}
