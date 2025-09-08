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
        if (subscribe_overlappings > this->meta_.overlapping_turns) {
            return std::make_pair(nullopt, this->meta_.overlapping_turns);
        } else if (subscribe_overlappings == this->meta_.overlapping_turns && index >= this->meta_.index_offset) {
            return std::make_pair(nullopt, this->meta_.overlapping_turns);
        }
        return std::make_pair(this->circle_[index], this->meta_.overlapping_turns);
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
}