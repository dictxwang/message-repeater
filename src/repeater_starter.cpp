#include <iostream>
#include <thread>
#include <chrono>
#include "config/repeater_config.h"
#include "logger/logger.h"
#include "combiner/global_context.h"
#include "publisher/publisher_acceptor.h"

int main(int argc, char const *argv[]) {

    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " config_file" << std::endl;
        return 0;
    }

    repeater::RepeaterConfig config;
    if (!config.loadRepeaterConfig(argv[1])) {
        std::cerr << "Load config error : " << argv[1] << std::endl;
        return 1;
    }

    // init logger
    spdlog::level::level_enum logger_level = static_cast<spdlog::level::level_enum>(config.logger_level);
    init_daily_file_log(config.logger_name, config.logger_file_path, logger_level, config.logger_max_files);

    repeater::GlobalContext global_context;
    global_context.init(config);

    publisher::PublisherBootstrap publisherBootstrap;
    publisherBootstrap.init(connection::SERVER_ROLE_PUBLISHER, config.publisher_listen_address, config.publisher_listen_port, config.publisher_max_connection);
    publisherBootstrap.start(config, global_context);

    while(true) {
        // std::cout << "agent starter keep running" << std::endl;
        // info_log("agent stater keep running");
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    return 0;
}
