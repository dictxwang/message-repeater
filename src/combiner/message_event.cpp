#include "message_event.h"

namespace repeater {

    void EventLoopWorker::init(event_callback_fn callback, void * args) {
        base = event_base_new();
        work_event = evuser_new(base, callback, args);
        event_add(work_event, nullptr);
    }

    void EventLoopWorker::submitWork(string topic) {
        {
            std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
            topic_queue.push(topic);
        }
        evuser_trigger(work_event);
    }

    vector<string> EventLoopWorker::popWorks() {
        vector<string> items;
        {
            std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
            while (!topic_queue.empty()) {
                items.push_back(topic_queue.front());
                topic_queue.pop();
            }
        }
        return items;
    }

    void EventLoopWorker::run() {
        event_base_dispatch(base);
    }

    void EventLoopWorker::stop() {
        event_base_loopbreak(base);
    }
}
