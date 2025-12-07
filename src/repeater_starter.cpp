#include <iostream>
#include <thread>
#include <chrono>
#include "config/repeater_config.h"
#include "logger/logger.h"
#include "combiner/global_context.h"
#include "publisher/publisher_acceptor.h"
#include "subscriber/subscriber_acceptor.h"
#include "layer/layer_connector.h"
#include "combiner/risk_controller.h"

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

    // init and start publisher bootstrap
    // must new bootstrap out of condition scope
    publisher::PublisherBootstrap publisherBootstrap;
    if (!config.disable_accept_publisher) {
        publisherBootstrap.init(connection::SERVER_ROLE_PUBLISHER, config.publisher_listen_address, config.publisher_listen_port, config.publisher_max_connection);
        publisherBootstrap.start(config, global_context);
    }

    // init and start subscriber bootstrap
    subscriber::SubscriberBootstrap subscriberBootstrap;
    subscriberBootstrap.init(connection::SERVER_ROLE_SUBSCRIBER, config.subscriber_listen_address, config.subscriber_listen_port, config.subscriber_max_connection);
    subscriberBootstrap.start(config, global_context);
    subscriberBootstrap.startMessageDispatch(global_context);

    // init and start layer subscribe
    if (config.enable_layer_subscribe) {
        layer::start_layer_replay(config, global_context);
    }

    // start watchdog
    if (config.enable_run_watchdog) {
        repeater::start_watchdog(config, global_context);
    }

    while(true) {
        // std::cout << "repeator starter keep running" << std::endl;
        // info_log("repeator stater keep running");
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    return 0;
}
