#ifndef _RISK_CONTROLLER_H_
#define _RISK_CONTROLLER_H_

#include <thread>
#include <chrono>
#include "config/repeater_config.h"
#include "global_context.h"
#include "logger/logger.h"

using namespace std;

namespace repeater {

    void start_watchdog(RepeaterConfig& config, GlobalContext& context);
    void watch_connections_and_circles(RepeaterConfig& config, GlobalContext& context);

    void send_warning_message(RepeaterConfig& config, GlobalContext& context, string message);
}
#endif