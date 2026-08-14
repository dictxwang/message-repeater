#ifndef _MESSAGE_EVENT_H_
#define _MESSAGE_EVENT_H_

#include <event2/event.h>
#include <functional>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <thread>
#include <chrono>
#include <queue>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include "logger/logger.h"
#include "util/common_tool.h"

using namespace std;

namespace repeater {

    class EventLoopWorker {

    public:
         EventLoopWorker() : id(0), base(nullptr), work_event(nullptr), write_event(nullptr),
            write_event_enabled(false), disable_duplicate_entries(false) {
            notify_pipe[0] = -1;
            notify_pipe[1] = -1;
        }
        ~EventLoopWorker() {
            if (this->work_event) {
                event_free(this->work_event);
            }
            if (this->write_event) {
                event_free(this->write_event);
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
        event* write_event;
        bool write_event_enabled;
        int notify_pipe[2];
        queue<string> work_queue;
        bool disable_duplicate_entries;
        unordered_map<string, bool> work_queue_status;
        shared_mutex rw_lock_;

    public:
        void init(event_callback_fn callback, void * args);
        void setDisableDuplicateEntries(bool disable);
        bool submitWork(string topic);
        bool popWork(string &topic);
        vector<string> popWorks();
        bool hasWorks();
        bool initWriteEvent(int socket_fd, event_callback_fn callback, void * args);
        bool enableWriteEvent();
        void disableWriteEvent();
        void run();
        void stop();
        bool notifyStartWork();
        bool notifyStopWork();
    };
}

#endif
