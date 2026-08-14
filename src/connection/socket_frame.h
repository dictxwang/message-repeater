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

    std::vector<char> encodeSocketFrame(const std::string &topic, const std::string &message);
    NonBlockingSendStatus sendSocketFrameNonBlocking(
        int client_fd,
        const std::vector<char> &frame,
        std::size_t &offset);

}

#endif
