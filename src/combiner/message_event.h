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

using namespace std;

namespace repeater {

    class EventLoopWorker {

    public:
        EventLoopWorker() {
        }
        ~EventLoopWorker() {
            if (work_event) {
                event_free(work_event);
            }
            if (base) {
                event_base_free(base);
            }
            if (notify_pipe[0] != -1) {
                close(notify_pipe[0]);
            }
            if (notify_pipe[1] != -1) {
                close(notify_pipe[1]);
            }
        }

    private:
        event_base* base;
        event* work_event;
        int notify_pipe[2];
        std::queue<string> topic_queue;
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
