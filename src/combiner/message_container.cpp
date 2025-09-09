#include "message_container.h"

namespace repeater {

    void MessageCircle::append(string message) {

        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        this->circle_[this->meta_.index_offset] = message;
        if (this->meta_.index_offset + 1 >= this->max_size_) {
            this->meta_.overlapping_turns += 1;
            this->meta_.index_offset = this->meta_.index_offset + 1 - this->max_size_;
        } else {
            this->meta_.index_offset += 1;
        }

        #ifdef OPEN_STD_DEBUG_LOG
            std::cout << "after append message to circle " << this->topic_ << ": overlapping=" << this->meta_.overlapping_turns << ",offset=" << this->meta_.index_offset << std::endl;
        #endif
        
    }

    pair<optional<string>, int> MessageCircle::getMessageAndOverlappings(int subscribe_overlappings, int index) {
        
        if (this->meta_.overlapping_turns == 0 && this->meta_.index_offset == 0) {
            // no data in cirle
            return std::make_pair(nullopt, 0);
        }
        
        if (subscribe_overlappings > this->meta_.overlapping_turns) {
            return std::make_pair(nullopt, this->meta_.overlapping_turns);
        } else if (subscribe_overlappings == this->meta_.overlapping_turns && index >= this->meta_.index_offset) {
            return std::make_pair(nullopt, this->meta_.overlapping_turns);
        }

        if (this->meta_.overlapping_turns == 0) {
            return std::make_pair(this->circle_[index], this->meta_.overlapping_turns);
        } else {
            if (this->meta_.index_offset == 0) {
                return std::make_pair(this->circle_[index], this->meta_.overlapping_turns - 1);
            } else {
                return std::make_pair(this->circle_[index], this->meta_.overlapping_turns);
            }
        }
    }

    CircleMeta MessageCircle::getMeta() {
        CircleMeta meta;
        meta.index_offset = this->meta_.index_offset;
        meta.overlapping_turns = this->meta_.overlapping_turns;
        return meta;
    }

    void MessageCircleComposite::init(int max_topic_numbers) {
        this->max_topic_numbers_ = max_topic_numbers;
    }

    bool MessageCircleComposite::createCircleIfAbsent(string topic, int circle_max_size) {

        if (this->topics_.find(topic) != this->topics_.end()) {
            return true;
        }

        if (this->topic_circles_.size() >= this->max_topic_numbers_) {
            return false;
        }

        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        if (this->topic_circles_.find(topic) == this->topic_circles_.end()) {
            this->topic_circles_[topic] = std::make_shared<MessageCircle>(topic, circle_max_size);
            this->topics_.insert(topic);
        }
        return true;
    }

    bool MessageCircleComposite::appendMessageToCircle(string topic, string message) {
        auto circle = this->topic_circles_.find(topic);
        if (circle != this->topic_circles_.end()) {
            circle->second->append(message);
            return true;
        } else {
            return false;
        }
    }

    optional<shared_ptr<MessageCircle>> MessageCircleComposite::getCircle(string topic) {
        auto circle = this->topic_circles_.find(topic);
        if (circle == this->topic_circles_.end()) {
            return nullopt;
        } else {
            return circle->second;
        }
    }

    vector<string> ConsumeRecord::getTopics() {
        return this->topics_;
    }

    optional<CircleMeta> ConsumeRecord::getMeta(string topic) {
        auto meta = this->topic_records_.find(topic);
        if (meta == this->topic_records_.end()) {
            return nullopt;
        } else {
            CircleMeta result;
            result.index_offset = meta->second.index_offset;
            result.overlapping_turns = meta->second.overlapping_turns;
            return result;
        }
    }

    void ConsumeRecord::updateMeta(string topic, int producer_overlapping) {
        auto meta = this->topic_records_.find(topic);
        if (meta == this->topic_records_.end()) {
            // not supported topic
        } else {
            if (meta->second.overlapping_turns < producer_overlapping) {
                meta->second.overlapping_turns = producer_overlapping;
            }
            if (meta->second.index_offset + 1 >= this->max_circle_size_) {
                meta->second.index_offset = 0;

                meta->second.overlapping_turns += 1;
            } else {
                meta->second.index_offset += 1;
            }

            this->topic_records_[topic] = meta->second;
        }
    }

    void ConsumeRecordComposite::init(int max_records_size) {
        this->max_records_size_ = max_records_size;
    }

    bool ConsumeRecordComposite::createRecordIfAbsent(string client_ip, int client_port, vector<string> topics, int max_circle_size) {
        
        if (topics.size() == 0) {
            return false;
        }

        if (this->consume_records_.size() >= this->max_records_size_) {
            return false;
        }

        string key = client_ip + ":" + std::to_string(client_port);

        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        auto record = this->consume_records_.find(key);
        if (record == this->consume_records_.end()) {
            shared_ptr<ConsumeRecord> c_record = std::make_shared<ConsumeRecord>(client_ip, client_port, topics, max_circle_size);
            this->consume_records_[key] = c_record;
        }
        
        #ifdef OPEN_STD_DEBUG_LOG
            std::cout << "after create consume records size is " << this->consume_records_.size() << std::endl;
        #endif
        
        return true;
    }

    optional<shared_ptr<ConsumeRecord>> ConsumeRecordComposite::getRecord(string client_ip, int client_port) {

        string key = client_ip + ":" + std::to_string(client_port);
        auto record = this->consume_records_.find(key);
        if (record == this->consume_records_.end()) {
            return nullopt;
        } else {
            return record->second;
        }
    }

    void ConsumeRecordComposite::removeRecord(string client_ip, int client_port) {

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