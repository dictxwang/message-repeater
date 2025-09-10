#include <iostream>
#include "client/repeater_publisher.h"

using namespace std;

int main(int argc, char const *argv[]) {
    
    string server_ip = "127.0.0.1";
    int server_port = 10001;
    string topic = "Sample0001";

    repeater_client::RepeaterPublisher publisher = repeater_client::RepeaterPublisher(server_ip, server_port);
    
    // could use in thread if you need
    thread publisher_thead([&publisher, &topic]{
        while (true) {
            // waiting for retry connection
            std::this_thread::sleep_for(std::chrono::seconds(5));

            // connect
            if (!publisher.createConnection()) {
                continue;
            }

            // send message (the message maybe from outer scope by channel or other method)
            // message could be normal string or json string
            int times = 0;
            while (times++ < 100) {
                std::this_thread::sleep_for(std::chrono::seconds(2));
                string message = "sample message-" + std::to_string(times);
                if (publisher.sendMessage(topic, message)) {
                    std::cout << "send message: " << topic << "," << message << std::endl;
                } else {
                    std::cout << "fail to send message, retry later" << std::endl;
                    break;
                }
            }
        }
    });
    publisher_thead.detach();
    
    while(true) {
        std::cout << "publisher starter keep running" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    return 0;
}
