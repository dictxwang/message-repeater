#ifndef _MESSAGE_CONTAINER_H_
#define _MESSAGE_CONTAINER_H_

#include <string>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <unordered_map>
#include <set>
#include <iostream>
#include <optional>

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


    class ConsumeRecord {
    public:
        ConsumeRecord(string client_ip, int client_port, vector<string> topics, int max_circle_size) {
            this->client_ip_ = client_ip;
            this->client_port_ = client_port;
            this->max_circle_size_ = max_circle_size;
            for (string topic : topics) {
                CircleMeta meta;
                this->topic_records_[topic] = meta;
            }
            for (auto [k, v] : this->topic_records_) {
                this->topics_.push_back(k);
            }
        }
        ~ConsumeRecord() {}

    private:
        string client_ip_;
        int client_port_;
        int max_circle_size_;
        vector<string> topics_;
        unordered_map<string, CircleMeta> topic_records_;

    public:
        vector<string> getTopics();
        optional<CircleMeta> getMeta(string topic);
        void updateMeta(string topic, int producer_overlapping);
    };

    class ConsumeRecordComposite {
    public:
        ConsumeRecordComposite() {}
        ~ConsumeRecordComposite() {}
    
    private:
        int max_records_size_;
        unordered_map<string, shared_ptr<ConsumeRecord>> consume_records_;
        shared_mutex rw_lock_;
    
    public:
        void init(int max_records_size);
        bool createRecordIfAbsent(string client_ip, int client_port, vector<string> topics, int max_circle_size);
        optional<shared_ptr<ConsumeRecord>> getRecord(string client_ip, int client_port);
        void removeRecord(string client_ip, int client_port);
    };

}

#endif