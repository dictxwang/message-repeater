#ifndef _PUBLISHER_ACCEPTOR_H_
#define _PUBLISHER_ACCEPTOR_H_

#include "connection/acceptor.h"
#include <iostream>
#include <cerrno>
#include <cstring>

namespace publisher {

    class PublisherBootstrap : public connection::AbstractBootstrap {
    public:
        PublisherBootstrap() {};
        ~PublisherBootstrap() {};
    
    protected:
        void acceptHandle(repeater::RepeaterConfig &config, repeater::GlobalContext &context, int client_fd, string client_ip, int client_port);
        void clearConnectionResource(repeater::GlobalContext &context, string client_ip, int client_port);
    };
}

#endif