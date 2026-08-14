#include "message_container.h"

#include <limits>
#include <stdexcept>

namespace repeater {

    void MessageCircle::append(string message) {

        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        if (this->next_sequence_ == std::numeric_limits<MessageSequence>::max()) {
            throw std::overflow_error("message sequence exhausted for topic " + this->topic_);
        }

        StoredMessage stored;
        stored.sequence = this->next_sequence_;
        stored.body = std::move(message);
        this->circle_[stored.sequence % this->circle_.size()] = std::move(stored);
        this->next_sequence_ += 1;

        #ifdef OPEN_STD_DEBUG_LOG
            std::cout << "after append message to circle " << this->topic_ << ": next_sequence=" << this->next_sequence_ << std::endl;
        #endif
        
    }

    MessageReadResult MessageCircle::read(
        MessageSequence consumer_sequence,
        bool send_latest,
        SubscriberOverrunPolicy overrun_policy) {

        std::shared_lock<std::shared_mutex> r_lock(this->rw_lock_);
        MessageReadResult result;
        result.next_sequence = consumer_sequence;
        result.producer_sequence = this->next_sequence_;
        result.oldest_available_sequence = this->next_sequence_ > this->circle_.size()
            ? this->next_sequence_ - this->circle_.size()
            : 0;

        if (consumer_sequence >= this->next_sequence_) {
            return result;
        }

        MessageSequence target_sequence = consumer_sequence;
        if (send_latest) {
            result.overrun = consumer_sequence < result.oldest_available_sequence;
            target_sequence = this->next_sequence_ - 1;
            result.skipped_messages = target_sequence - consumer_sequence;
        } else if (consumer_sequence < result.oldest_available_sequence) {
            result.overrun = true;
            if (overrun_policy == SubscriberOverrunPolicy::Disconnect) {
                result.status = MessageReadStatus::Disconnect;
                result.skipped_messages = result.oldest_available_sequence - consumer_sequence;
                return result;
            }
            if (overrun_policy == SubscriberOverrunPolicy::Latest) {
                target_sequence = this->next_sequence_ - 1;
            } else {
                target_sequence = result.oldest_available_sequence;
            }
            result.skipped_messages = target_sequence - consumer_sequence;
        }

        const auto& slot = this->circle_[target_sequence % this->circle_.size()];
        if (!slot.has_value() || slot->sequence != target_sequence) {
            result.overrun = true;
            result.status = MessageReadStatus::Disconnect;
            return result;
        }

        result.status = MessageReadStatus::Message;
        result.message = slot->body;
        result.message_sequence = target_sequence;
        result.next_sequence = (send_latest ||
            (result.overrun && overrun_policy == SubscriberOverrunPolicy::Latest))
            ? this->next_sequence_
            : target_sequence + 1;
        return result;
    }

    CircleMeta MessageCircle::getMeta() {
        std::shared_lock<std::shared_mutex> r_lock(this->rw_lock_);
        CircleMeta meta;
        meta.next_sequence = this->next_sequence_;
        meta.oldest_available_sequence = this->next_sequence_ > this->circle_.size()
            ? this->next_sequence_ - this->circle_.size()
            : 0;
        meta.overlapping_turns = this->next_sequence_ / this->circle_.size();
        meta.index_offset = this->next_sequence_ % this->circle_.size();
        return meta;
    }

    void MessageCircleComposite::init(int max_topic_numbers) {
        this->max_topic_numbers_ = max_topic_numbers;
    }

    bool MessageCircleComposite::createCircleIfAbsent(string topic, int circle_max_size) {

        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        if (this->topics_.find(topic) != this->topics_.end()) {
            return true;
        }

        if (this->topic_circles_.size() >= this->max_topic_numbers_) {
            return false;
        }

        if (this->topic_circles_.find(topic) == this->topic_circles_.end()) {
            this->topic_circles_[topic] = std::make_shared<MessageCircle>(topic, circle_max_size);
            this->topics_.insert(topic);
        }
        return true;
    }

    bool MessageCircleComposite::appendMessageToCircle(string topic, string message) {
        std::shared_lock<std::shared_mutex> r_lock(this->rw_lock_);
        auto circle = this->topic_circles_.find(topic);
        if (circle != this->topic_circles_.end()) {
            circle->second->append(message);
            return true;
        } else {
            return false;
        }
    }

    optional<shared_ptr<MessageCircle>> MessageCircleComposite::getCircle(string topic) {
        std::shared_lock<std::shared_mutex> r_lock(this->rw_lock_);
        auto circle = this->topic_circles_.find(topic);
        if (circle == this->topic_circles_.end()) {
            return nullopt;
        } else {
            return circle->second;
        }
    }

    vector<string> MessageCircleComposite::getTopics() {
        std::shared_lock<std::shared_mutex> r_lock(this->rw_lock_);
        vector<string> result;
        for (string topic : this->topics_) {
            result.push_back(topic);
        }
        return result;
    }

    vector<string> ConsumeRecord::getTopics() {
        std::shared_lock<std::shared_mutex> r_lock(this->rw_lock_);
        return this->topics_;
    }

    optional<ConsumeMeta> ConsumeRecord::getMeta(string topic) {
        std::shared_lock<std::shared_mutex> r_lock(this->rw_lock_);
        auto meta = this->topic_records_.find(topic);
        if (meta == this->topic_records_.end()) {
            return nullopt;
        } else {
            return meta->second;
        }
    }

    void ConsumeRecord::initialize(string topic, MessageSequence producer_sequence) {
        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        auto meta = this->topic_records_.find(topic);
        if (meta == this->topic_records_.end() || meta->second.initialized) {
            return;
        }
        meta->second.next_sequence = producer_sequence;
        meta->second.initialized = true;
    }

    void ConsumeRecord::updateSequence(string topic, MessageSequence next_sequence) {
        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        auto meta = this->topic_records_.find(topic);
        if (meta == this->topic_records_.end()) {
            return;
        }
        meta->second.next_sequence = next_sequence;
        meta->second.initialized = true;
    }

    void ConsumeRecordComposite::init(int max_records_size) {
        this->max_records_size_ = max_records_size;
    }

    bool ConsumeRecordComposite::createNewRecord(string client_ip, int client_port, vector<string> topics, int max_circle_size) {
        
        if (topics.size() == 0) {
            return false;
        }

        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);

        if (this->consume_records_.size() >= this->max_records_size_) {
            return false;
        }
        string key = client_ip + ":" + std::to_string(client_port);
        // auto record = this->consume_records_.find(key);
        // if (record == this->consume_records_.end()) {
        //     shared_ptr<ConsumeRecord> c_record = std::make_shared<ConsumeRecord>(client_ip, client_port, topics, max_circle_size);
        //     this->consume_records_[key] = c_record;
        // }

        shared_ptr<ConsumeRecord> c_record = std::make_shared<ConsumeRecord>(client_ip, client_port, topics, max_circle_size);
        this->consume_records_[key] = c_record;
        
        #ifdef OPEN_STD_DEBUG_LOG
            std::cout << "after create consume records size is " << this->consume_records_.size() << std::endl;
        #endif
        
        return true;
    }

    optional<shared_ptr<ConsumeRecord>> ConsumeRecordComposite::getRecord(string client_ip, int client_port) {

        std::shared_lock<std::shared_mutex> w_lock(this->rw_lock_);
        string key = client_ip + ":" + std::to_string(client_port);
        auto record = this->consume_records_.find(key);
        if (record == this->consume_records_.end()) {
            return nullopt;
        } else {
            return record->second;
        }
    }

    void ConsumeRecordComposite::removeRecord(string client_ip, int client_port) {

        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        string key = client_ip + ":" + std::to_string(client_port);
        auto record = this->consume_records_.find(key);
        if (record != this->consume_records_.end()) {
            this->consume_records_.erase(key);

            #ifdef OPEN_STD_DEBUG_LOG
                std::cout << "after remove consume records size is " << this->consume_records_.size() << std::endl;
            #endif
        }
    }
}
