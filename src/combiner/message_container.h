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
#include <cstdint>
#include <stdexcept>
#include "combiner/message_policy.h"

using namespace std;

namespace repeater {

    using MessageSequence = uint64_t;

    struct StoredMessage {
        MessageSequence sequence = 0;
        string body;
    };

    struct CircleMeta {
        uint64_t overlapping_turns = 0;
        size_t index_offset = 0;
        MessageSequence next_sequence = 0;
        MessageSequence oldest_available_sequence = 0;
    };

    enum class MessageReadStatus {
        NoMessage,
        Message,
        Disconnect
    };

    struct MessageReadResult {
        MessageReadStatus status = MessageReadStatus::NoMessage;
        optional<string> message;
        MessageSequence message_sequence = 0;
        MessageSequence next_sequence = 0;
        MessageSequence producer_sequence = 0;
        MessageSequence oldest_available_sequence = 0;
        MessageSequence skipped_messages = 0;
        bool overrun = false;
    };

    struct ConsumeMeta {
        MessageSequence next_sequence = 0;
        bool initialized = false;
    };

    class MessageCircle {
    public:
        MessageCircle(string topic, int max_size) {
            if (max_size <= 0) {
                throw invalid_argument("message circle size must be greater than zero");
            }
            this->topic_ = topic;
            this->max_size_ = max_size;
            for (int i = 0; i < max_size; i++) {
                this->circle_.push_back(nullopt);
            }

        }
        ~MessageCircle() {}
    
    private:
        int max_size_;
        string topic_;
        vector<optional<StoredMessage>> circle_;
        MessageSequence next_sequence_ = 0;

        shared_mutex rw_lock_;

    public:
        void append(string message);
        MessageReadResult read(
            MessageSequence consumer_sequence,
            bool send_latest,
            SubscriberOverrunPolicy overrun_policy);

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
        vector<string> getTopics();
    };


    class ConsumeRecord {
    public:
        ConsumeRecord(string client_ip, int client_port, vector<string> topics, int max_circle_size) {
            this->client_ip_ = client_ip;
            this->client_port_ = client_port;
            (void)max_circle_size;
            for (string topic : topics) {
                ConsumeMeta meta;
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
        vector<string> topics_;
        unordered_map<string, ConsumeMeta> topic_records_;
        shared_mutex rw_lock_;

    public:
        vector<string> getTopics();
        optional<ConsumeMeta> getMeta(string topic);
        void initialize(string topic, MessageSequence producer_sequence);
        void updateSequence(string topic, MessageSequence next_sequence);
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
        bool createNewRecord(string client_ip, int client_port, vector<string> topics, int max_circle_size);
        optional<shared_ptr<ConsumeRecord>> getRecord(string client_ip, int client_port);
        void removeRecord(string client_ip, int client_port);
    };

}

#endif
