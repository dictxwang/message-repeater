#include "message_event.h"
#include <iostream>

namespace repeater {

    void EventLoopWorker::init(event_callback_fn callback, void * args) {
        if (pipe(this->notify_pipe) == -1) {
            warn_log("fail to create pipe for event loop worker");
        } else {
            evutil_make_socket_nonblocking(this->notify_pipe[0]);
            evutil_make_socket_nonblocking(this->notify_pipe[1]);
        }
        this->base = event_base_new();
        this->work_event = event_new(base, this->notify_pipe[0], EV_READ | EV_PERSIST, callback, args);
        event_add(work_event, nullptr);
    }

    void EventLoopWorker::submitWork(string topic) {
        std::cout << "submit 001" << std::endl;
        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        std::cout << "submit 002" << std::endl;
        this->topic_queue.push(topic);
        std::cout << "submit 003" << std::endl;
    }

    vector<string> EventLoopWorker::popWorks() {
        vector<string> items;
        std::cout << "pop 001" << std::endl;
        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        std::cout << "pop 002" << std::endl;
        while (!this->topic_queue.empty()) {
            items.push_back(topic_queue.front());
            this->topic_queue.pop();
        }
        return items;
    }

    void EventLoopWorker::run() {
        event_base_dispatch(this->base);
    }

    void EventLoopWorker::stop() {
        event_base_loopbreak(this->base);
    }
    bool EventLoopWorker::notifyWork() {
        char byte = 1;
        if (write(notify_pipe[1], &byte, 1) != 1) {
            warn_log("fail to notify worker");
            return false;
        } else {
            return true;
        }
    }
}
