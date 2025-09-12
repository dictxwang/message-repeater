#include "risk_controller.h"

using namespace std;

namespace repeater {

    void start_watchdog(RepeaterConfig& config, GlobalContext& context) {
        thread circle_thead(watch_topic_circles, ref(config), ref(context));
        circle_thead.detach();
        info_log("[watchdog] start thread of watching topic circles");
    }

    void watch_topic_circles(RepeaterConfig& config, GlobalContext& context) {
        while (true) {
            this_thread::sleep_for(chrono::minutes(2));
            vector <string> topics = context.get_message_circle_composite()->getTopics();
            for (string topic : topics) {
                auto circle = context.get_message_circle_composite()->getCircle(topic);
                if (!circle.has_value()) {
                    warn_log("[watchdog] message circle not found for topic {}", topic);
                } else {
                    CircleMeta meta = circle.value()->getMeta();
                    info_log("[watchdog] message circle for topic {} overlapping_turns={}, index_offset={}", topic, meta.overlapping_turns, meta.index_offset);
                }
            }
        }
    }

    void send_warning_message(RepeaterConfig& config, GlobalContext& context, string message) {
        if (!config.tg_send_message) {
            warn_log("close send tg messag: {}", message);
        } else {
            message = "[" + config.process_node_name + "] " + message;
            pair<int, string> res = context.get_tg_bot().send_message(config.tg_chat_id, message);
            if (res.first != 0) {
                err_log("fail to send tg message: {} {}", res.first, res.second);
            }
        }
    }
}