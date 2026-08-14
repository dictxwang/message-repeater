#ifndef _CONNECTION_SOCKET_FRAME_H_
#define _CONNECTION_SOCKET_FRAME_H_

#include <cstddef>
#include <string>
#include <vector>

namespace connection {

    enum class NonBlockingSendStatus {
        Complete,
        WouldBlock,
        Error
    };

    enum class SocketFrameReadStatus {
        Complete,
        Timeout,
        Closed,
        TooLarge,
        Error
    };

    struct SocketFrameReadResult {
        SocketFrameReadStatus status;
        std::string topic;
        std::string message;
    };

    std::vector<char> encodeSocketFrame(const std::string &topic, const std::string &message);
    bool sendSocketFrameBlocking(int client_fd, const std::vector<char> &frame);
    NonBlockingSendStatus sendSocketFrameNonBlocking(
        int client_fd,
        const std::vector<char> &frame,
        std::size_t &offset);
    SocketFrameReadResult readSocketFrameBlocking(
        int client_fd,
        std::size_t max_topic_size,
        std::size_t max_message_size);

}

#endif
