#include "message_event.h"

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
        this->id = common_tools::get_current_micro_epoch();
        this->disable_duplicate_entries = false;
        info_log("create event loop worker which id is {}", this->id);
    }

    void EventLoopWorker::setDisableDuplicateEntries(bool disable) {
        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        this->disable_duplicate_entries = disable;
    }

    void EventLoopWorker::clearWorkQueueStatus(string topic) {
        if (!this->disable_duplicate_entries) {
            return;
        }
        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        this->work_queue_status[topic] = false;
    }

    void EventLoopWorker::submitWork(string topic) {
        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        if (this->disable_duplicate_entries) {
            auto status = this->work_queue_status.find(topic);
            if (status != this->work_queue_status.end() && status->second) {
                return;
            }
        }
        if (this->work_queue.size() >= 150) {
            warn_log("work queue size is {} which id is {}", this->work_queue.size(), this->id);
            return;
        }
        this->work_queue.push(topic);
        if (this->disable_duplicate_entries) {
            this->work_queue_status[topic] = true;
        }
    }

    vector<string> EventLoopWorker::popWorks() {
        vector<string> items;
        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        while (!this->work_queue.empty()) {
            string head = this->work_queue.front();
            items.push_back(head);
            this->work_queue.pop();
        }
        return items;
    }

    void EventLoopWorker::run() {
        event_base_dispatch(this->base);
    }

    void EventLoopWorker::stop() {
        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        event_base_loopbreak(this->base);
        this->work_queue_status.clear();
        while (!this->work_queue.empty()) {
            this->work_queue.pop();
        }
    }

    bool EventLoopWorker::notifyStartWork() {
        char byte = 1;
        ssize_t write_result = write(notify_pipe[1], &byte, 1);
        if (write_result != 1) {
            int err = errno;
            warn_log("fail to notify event loop to start: worker_id={}, fd={}, write_result={}, errno={}, error={}",
                this->id, notify_pipe[1], write_result, err, strerror(err));
            return false;
        } else {
            return true;
        }
    }
    bool EventLoopWorker::notifyStopWork() {
        char byte = 5;
        ssize_t write_result = write(notify_pipe[1], &byte, 1);
        if (write_result != 1) {
            int err = errno;
            warn_log("fail to notify event loop to stop: worker_id={}, fd={}, write_result={}, errno={}, error={}",
                this->id, notify_pipe[1], write_result, err, strerror(err));
            return false;
        } else {
            return true;
        }
    }
}
