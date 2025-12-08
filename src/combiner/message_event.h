#ifndef _MESSAGE_EVENT_H_
#define _MESSAGE_EVENT_H_

#include <event2/event.h>
#include <functional>
#include <signal.h>
#include <thread>
#include <chrono>
#include <queue>
#include <vector>
#include <mutex>
#include <shared_mutex>

using namespace std;

namespace repeater {

    class EventLoopWorker {

        // static void work_callback(evutil_socket_t fd, short what, void* arg) {
        //     auto* worker = static_cast<EventLoopWorker*>(arg);
        //     worker->processWork();
        // }

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
        }

    private:
        event_base* base;
        event* work_event;
        std::queue<string> topic_queue;
        shared_mutex rw_lock_;

    public:
        void init(event_callback_fn callback, void * args);
        void submitWork(string topic);
        vector<string> popWorks();
        void run();
        void stop();
        event* getWorkEvent();
    };
}

#endif
