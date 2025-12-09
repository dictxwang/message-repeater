#ifndef _MESSAGE_EVENT_H_
#define _MESSAGE_EVENT_H_

#include <event2/event.h>
#include <functional>
#include <signal.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <queue>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include "logger/logger.h"
#include "util/common_tool.h"

using namespace std;

namespace repeater {

    class EventLoopWorker {

    public:
        EventLoopWorker() {
        }
        ~EventLoopWorker() {
            if (this->work_event) {
                event_free(this->work_event);
            }
            if (this->base) {
                event_base_free(this->base);
            }
            if (this->notify_pipe[0] != -1) {
                close(this->notify_pipe[0]);
            }
            if (this->notify_pipe[1] != -1) {
                close(this->notify_pipe[1]);
            }
            info_log("destroy event loop worker which id is {}", this->id);
        }

    private:
        uint64_t id;
        event_base* base;
        event* work_event;
        int notify_pipe[2];
        queue<string> topic_queue;
        shared_mutex rw_lock_;

    public:
        void init(event_callback_fn callback, void * args);
        void submitWork(string topic);
        vector<string> popWorks();
        void run();
        void stop();
        bool notifyStartWork();
        bool notifyStopWork();
    };
}

#endif
