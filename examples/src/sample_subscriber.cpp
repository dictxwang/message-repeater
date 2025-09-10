#include <iostream>
#include "client/repeater_subscriber.h"

using namespace std;

int main(int argc, char const *argv[]) {
    
    vector<string> topics;
    topics.push_back("Sample0001");
    topics.push_back("Sample0002");

    string server_ip = "127.0.0.1";
    int server_port = 20001;

    repeater_client::RepeaterSubscriber subscriber = repeater_client::RepeaterSubscriber(server_ip, server_port);
    
    // could use in thread if you need
    thread subcribe_thread([&subscriber, &topics] {
        while (true) {
            // waiting for retry connection
            std::this_thread::sleep_for(std::chrono::seconds(5));

            // connect
            if (!subscriber.createConnection()) {
                continue;
            }

            // waiting for connection ready
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // subscribe
            if (!subscriber.subscribe(topics)) {
                continue;
            }

            while (true) {
                // readMessage will be block
                auto message = subscriber.readMessage();
                if (!message.has_value()) {
                    break;
                }
                if (message.value().first == repeater_client::MESSAGE_OP_TOPIC_PONG) {
                    std::cout << "receive pong message: " << message.value().second << std::endl;
                } else {
                    std::cout << "receive normal message: " << message.value().first << "," << message.value().second << std::endl;
                    // use the message in your business
                }
            }
        }
    });
    subcribe_thread.detach();
    
    while(true) {
        std::cout << "subscriber starter keep running" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    return 0;
}
