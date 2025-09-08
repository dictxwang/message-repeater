#ifndef _MESSAGE_CONTAINER_H_
#define _MESSAGE_CONTAINER_H_

#include <string>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <unordered_map>
#include <set>

using namespace std;

namespace repeater {

    struct CircleMeta {
        int overlapping_turns = 0;
        int index_offset = 0;
    };

    class MessageCircle {
    public:
        MessageCircle(string topic, int max_size) {
            this->topic_ = topic;
            this->max_size_ = max_size;
            for (int i = 0; i < max_size; i++) {
                this->circle_.push_back("");
            }

        }
        ~MessageCircle() {}
    
    private:
        int max_size_;
        string topic_;
        vector<string> circle_;
        CircleMeta meta_;

        shared_mutex rw_lock_;

    public:
        void append(string message);
        pair<optional<string>, int> getMessageAndOverlappings(int subscribe_overlappings, int index);

        CircleMeta getMeta();
    };

    class MessageCircleComposite {
    public:
        MessageCircleComposite() {};
        ~MessageCircleComposite() {};
    
    private:
        int max_topic_numbers_;
        set<string> topics_;
        unordered_map<string, shared_ptr<MessageCircle>> topic_circles_;
        shared_mutex rw_lock_;
    
    public:
        void init(int max_topic_numbers);
        bool createCircleIfAbsent(string topic, int circle_max_size);
        bool appendMessageToCircle(string topic, string message);
        optional<shared_ptr<MessageCircle>> getCircle(string topic);
    };
}

#endif