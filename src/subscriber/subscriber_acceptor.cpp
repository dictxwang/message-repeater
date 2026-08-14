#include "subscriber_acceptor.h"

#include <poll.h>

using namespace std;

namespace subscriber {

    namespace {

        const char* overrunPolicyName(repeater::SubscriberOverrunPolicy policy) {
            switch (policy) {
                case repeater::SubscriberOverrunPolicy::Latest:
                    return "latest";
                case repeater::SubscriberOverrunPolicy::Oldest:
                    return "oldest";
                case repeater::SubscriberOverrunPolicy::Disconnect:
                    return "disconnect";
            }
            return "unknown";
        }

        bool receiveExact(
            int client_fd,
            void *buffer,
            size_t length,
            const shared_ptr<atomic_bool> &connection_alived) {

            size_t received = 0;
            char *output = static_cast<char*>(buffer);
            while (received < length && connection_alived->load()) {
                ssize_t count = recv(client_fd, output + received, length - received, 0);
                if (count > 0) {
                    received += static_cast<size_t>(count);
                    continue;
                }
                if (count == 0) {
                    return false;
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    return false;
                }

                pollfd socket_event {client_fd, POLLIN, 0};
                int poll_result = poll(&socket_event, 1, 100);
                if (poll_result < 0 && errno != EINTR) {
                    return false;
                }
                if (poll_result > 0 &&
                    (socket_event.revents & (POLLERR | POLLNVAL)) != 0) {
                    return false;
                }
                if (poll_result > 0 &&
                    (socket_event.revents & POLLHUP) != 0 &&
                    (socket_event.revents & POLLIN) == 0) {
                    return false;
                }
            }
            return received == length;
        }

    }

    void SubscriberBootstrap::startEventLoopForDispatching(repeater::GlobalContext &context) {

        shared_ptr<repeater::EventLoopWorker> eventLoop = context.get_dispatch_event_loop_worker();
        DispatchingEventWorkArguments *eventArguments = new DispatchingEventWorkArguments {
            context.get_dispatch_event_loop_worker(),
            this,
            context
        };

        eventLoop->init([](evutil_socket_t ev_fd, short flags, void * args){
            DispatchingEventWorkArguments* arguments = static_cast<DispatchingEventWorkArguments*>(args);

            char buf;
            // Read from pipe to clear it
            while (read(ev_fd, &buf, 1) == 1) {}

            vector<string> topics = arguments->eventLoop->popWorks();
            if (topics.size() == 0) {
                return;
            }

            for (string topic : topics) {
                arguments->subscriber->dispatchMessage(topic);
            }

        }, eventArguments);
        eventLoop->setDisableDuplicateEntries(true);

        thread event_thread([eventLoop, eventArguments] {
            info_log("start run event loop for dispatching message to all subscribers");
            eventLoop->run();
            delete eventArguments;  // Clean up the heap-allocated arguments
            info_log("stop run event loop for dispatching message to all subscribers");
        });
        event_thread.detach();
        this_thread::sleep_for(chrono::seconds(1));
    }

    void SubscriberBootstrap::startEventLoopForAcceptHandle(repeater::GlobalContext &context) {
        // this->startMessageDispatchingThread(context);
        this->startConnectionDetectingThread();
    }

    // void SubscriberBootstrap::startMessageDispatchingThread(repeater::GlobalContext &context) {
        
    //     thread dispatching_thread([this, &context] {

    //         while (true) {
    //             this_thread::sleep_for(chrono::microseconds(5));
    //             auto topics = context.pop_message_topics_for_event_loop();
    //             if (topics.size() == 0) {
    //                 continue;
    //             }
    //             for (string topic : topics) {
    //                 this->dispatchMessage(topic);
    //             }
    //         }
    //     });
    //     dispatching_thread.detach();
    //     info_log("subscriber start message dispatching thread");
    // }

    void SubscriberBootstrap::dispatchMessage(string topic) {
        std::shared_lock<std::shared_mutex> w_lock(this->rw_lock_);
        auto connections = this->topic_connection_map_.find(topic);
        if (connections == this->topic_connection_map_.end()) {
            return;
        }

        for (string connection : connections->second) {
            auto event_loop = this->connection_event_loop_map_.find(connection);
            if (event_loop != this->connection_event_loop_map_.end()) {
                bool queued = event_loop->second->submitWork(topic);
                if (queued) {
                    bool notifyResult = event_loop->second->notifyStartWork();
                    if (!notifyResult) {
                        warn_log("fail to notify event loop to start for subscriber {} which topic is {}", connection, topic);
                    }
                }
            }
        }
    }

    TopicDeliveryStatus SubscriberBootstrap::deliverTopic(
        repeater::RepeaterConfig &config,
        shared_ptr<repeater::ConsumeRecord> record,
        shared_ptr<repeater::MessageCircle> circle,
        int client_fd,
        string topic,
        string client_ip,
        int client_port,
        shared_ptr<SubscriberOutputState> output_state) {

        if (output_state->pending_frame.has_value()) {
            return TopicDeliveryStatus::WouldBlock;
        }

        optional<repeater::ConsumeMeta> meta = record->getMeta(topic);
        if (!meta.has_value()) {
            return TopicDeliveryStatus::NoMessage;
        }

        if (!meta->initialized) {
            repeater::CircleMeta producer_meta = circle->getMeta();
            record->initialize(topic, producer_meta.next_sequence);
            return TopicDeliveryStatus::NoMessage;
        }

        repeater::MessageReadResult result = circle->read(
            meta->next_sequence,
            config.subscriber_always_send_latest,
            config.subscriber_overrun_policy);

        if (result.status == repeater::MessageReadStatus::Disconnect) {
            warn_log(
                "subscriber overrun disconnect for {}:{} topic={} consumer_sequence={} oldest_sequence={} producer_sequence={} skipped={}",
                client_ip,
                client_port,
                topic,
                meta->next_sequence,
                result.oldest_available_sequence,
                result.producer_sequence,
                result.skipped_messages);
            return TopicDeliveryStatus::Disconnect;
        }
        if (result.status == repeater::MessageReadStatus::NoMessage || !result.message.has_value()) {
            return TopicDeliveryStatus::NoMessage;
        }

        if (result.overrun) {
            warn_log(
                "subscriber overrun recovered for {}:{} topic={} policy={} consumer_sequence={} oldest_sequence={} producer_sequence={} message_sequence={} skipped={}",
                client_ip,
                client_port,
                topic,
                config.subscriber_always_send_latest
                    ? "latest_only"
                    : overrunPolicyName(config.subscriber_overrun_policy),
                meta->next_sequence,
                result.oldest_available_sequence,
                result.producer_sequence,
                result.message_sequence,
                result.skipped_messages);
        }

        if (result.message->empty()) {
            record->updateSequence(topic, result.next_sequence);
            return TopicDeliveryStatus::Delivered;
        }

        output_state->pending_frame = PendingSubscriberFrame {
            connection::encodeSocketFrame(topic, result.message.value()),
            0,
            true,
            topic,
            result.next_sequence
        };

        string completed_topic;
        PendingWriteStatus write_status = this->flushPendingFrame(
            output_state, record, client_fd, completed_topic);
        if (write_status == PendingWriteStatus::Error) {
            return TopicDeliveryStatus::Disconnect;
        }
        if (write_status == PendingWriteStatus::WouldBlock) {
            return TopicDeliveryStatus::WouldBlock;
        }
        return write_status == PendingWriteStatus::Complete
            ? TopicDeliveryStatus::Delivered
            : TopicDeliveryStatus::NoMessage;
    }

    PendingWriteStatus SubscriberBootstrap::flushPendingFrame(
        shared_ptr<SubscriberOutputState> output_state,
        shared_ptr<repeater::ConsumeRecord> record,
        int client_fd,
        string &completed_topic) {

        completed_topic.clear();
        if (!output_state->pending_frame.has_value()) {
            std::lock_guard<std::mutex> lock(output_state->control_mutex);
            if (!output_state->control_frames.empty()) {
                vector<char> frame = std::move(output_state->control_frames.front());
                output_state->control_frames.pop_front();
                output_state->pending_frame = PendingSubscriberFrame {
                    std::move(frame), 0, false, "", 0
                };
            }
        }
        if (!output_state->pending_frame.has_value()) {
            return PendingWriteStatus::Idle;
        }

        PendingSubscriberFrame &frame = output_state->pending_frame.value();
        connection::NonBlockingSendStatus status = connection::sendSocketFrameNonBlocking(
            client_fd, frame.data, frame.offset);
        if (status == connection::NonBlockingSendStatus::WouldBlock) {
            return PendingWriteStatus::WouldBlock;
        }
        if (status == connection::NonBlockingSendStatus::Error) {
            return PendingWriteStatus::Error;
        }

        if (frame.advances_sequence && record == nullptr) {
            return PendingWriteStatus::Error;
        }
        if (frame.advances_sequence) {
            record->updateSequence(frame.topic, frame.next_sequence);
            completed_topic = frame.topic;
        }
        output_state->pending_frame.reset();
        return PendingWriteStatus::Complete;
    }

    bool SubscriberBootstrap::enqueueControlFrame(
        shared_ptr<SubscriberOutputState> output_state,
        const string &topic,
        const string &message) {

        vector<char> frame = connection::encodeSocketFrame(topic, message);
        {
            std::lock_guard<std::mutex> lock(output_state->control_mutex);
            if (output_state->control_frames.size() >= SubscriberOutputState::MAX_CONTROL_FRAMES) {
                warn_log("subscriber control output queue is full");
                return false;
            }
            output_state->control_frames.push_back(std::move(frame));
        }

        if (output_state->event_loop != nullptr &&
            !output_state->event_loop->notifyStartWork()) {
            warn_log("fail to notify subscriber output event loop");
            return false;
        }
        return true;
    }

    void SubscriberBootstrap::startConnectionDetectingThread() {

        thread detecting_thread([this] {

            while (true) {
                this_thread::sleep_for(chrono::milliseconds(1));
                
                vector<shared_ptr<ConnectionDetectingArguments>> argumentsList;
                std::shared_lock<std::shared_mutex> r_lock(this->rw_lock_);
                for (auto [k, arguments] : this->connection_detecting_args_map_) {
                    argumentsList.emplace_back(arguments);
                }
                r_lock.unlock();
                for (auto arguments : argumentsList) {
                    if (arguments->detecting_finished) {
                        continue;
                    }
                    bool connection_broken = false;
                    if (!arguments->connection_alived->load()) {
                        connection_broken = true;
                    } else if (!this->isConnectionExists(arguments->client_ip, arguments->client_port)) {
                        warn_log("subscriber connection not exists for {}:{}", arguments->client_ip, arguments->client_port);
                        connection_broken = true;
                    } else if (!this->isSubscribed(arguments->client_ip, arguments->client_port)) {
                        continue;
                    }

                    if (connection_broken) {
                        info_log("subscribe connection be detected has broken for {}:{}", arguments->client_ip, arguments->client_port);
                        shutdown(arguments->client_fd, SHUT_RDWR);
                        this->killAlive(arguments->client_ip, arguments->client_port);
                        bool notifyResult = arguments->eventLoop->notifyStopWork();
                        if (!notifyResult) {
                            warn_log("fail to notify event loop to stop for subscriber {}:{}", arguments->client_ip, arguments->client_port);
                        }
                        arguments->connection_alived->store(false);
                        arguments->detecting_finished = true;
                    }
                }
            }
        });
        detecting_thread.detach();
        info_log("subscriber start connection detecting thread");
    }

    void SubscriberBootstrap::startAcceptHandleNormalWritingThread(
        repeater::RepeaterConfig &config,
        repeater::GlobalContext &context,
        int client_fd,
        string client_ip,
        int client_port,
        shared_ptr<atomic_bool> connection_alived,
        shared_ptr<SubscriberOutputState> output_state) {
        
        thread write_thread([this, client_fd, client_ip, client_port, &config, &context, connection_alived, output_state] {
            shared_ptr<repeater::ConsumeRecord> record;

            while (connection_alived->load()) {
                if (!this->isConnectionExists(client_ip, client_port)) {
                    info_log("subscriber connection not exists for {}:{}", client_ip, client_port);
                    break;
                }

                if (record == nullptr && this->isSubscribed(client_ip, client_port)) {
                    optional<shared_ptr<repeater::ConsumeRecord>> current_record =
                        context.get_consume_record_composite()->getRecord(client_ip, client_port);
                    if (current_record.has_value()) {
                        record = current_record.value();
                    }
                }

                string completed_topic;
                PendingWriteStatus write_status = this->flushPendingFrame(
                    output_state, record, client_fd, completed_topic);
                if (write_status == PendingWriteStatus::Error) {
                    break;
                }
                if (write_status == PendingWriteStatus::WouldBlock) {
                    this_thread::sleep_for(chrono::milliseconds(1));
                    continue;
                }
                if (write_status == PendingWriteStatus::Complete) {
                    continue;
                }

                if (!this->isSubscribed(client_ip, client_port)) {
                    this_thread::sleep_for(chrono::microseconds(100));
                    continue;
                }
                if (record == nullptr) {
                    break;
                }

                bool disconnect = false;
                bool would_block = false;
                for (string topic : record->getTopics()) {
                    optional<shared_ptr<repeater::MessageCircle>> circle = context.get_message_circle_composite()->getCircle(topic);
                    if (!circle.has_value()) {
                        continue;
                    }
                    TopicDeliveryStatus status = this->deliverTopic(
                        config,
                        record,
                        circle.value(),
                        client_fd,
                        topic,
                        client_ip,
                        client_port,
                        output_state);
                    if (status == TopicDeliveryStatus::Disconnect) {
                        this->removeSubscribed(client_ip, client_port);
                        connection_alived->store(false);
                        warn_log("subscriber delivery failed and remove subscribed for {}:{}", client_ip, client_port);
                        disconnect = true;
                        break;
                    }
                    if (status == TopicDeliveryStatus::WouldBlock) {
                        would_block = true;
                        break;
                    }
                }
                if (disconnect) {
                    break;
                }
                this_thread::sleep_for(would_block ? chrono::milliseconds(1) : chrono::microseconds(100));
            }
            shutdown(client_fd, SHUT_RDWR);
            this->killAlive(client_ip, client_port);
            connection_alived->store(false);
        });
        write_thread.detach();
        info_log("subscriber run normal writing thread for {}:{}", client_ip, client_port);
    }

    void SubscriberBootstrap::startAcceptHandleEventLoopWritingThread(
        repeater::RepeaterConfig &config,
        repeater::GlobalContext &context,
        int client_fd,
        string client_ip,
        int client_port,
        shared_ptr<atomic_bool> connection_alived,
        shared_ptr<SubscriberOutputState> output_state) {
        
        shared_ptr<repeater::EventLoopWorker> eventLoop = std::make_shared<repeater::EventLoopWorker>();
        output_state->event_loop = eventLoop;

        WritingEventWorkArguments *eventArguments = new WritingEventWorkArguments {
            eventLoop,
            this,
            client_fd,
            client_ip,
            client_port,
            config,
            context,
            connection_alived,
            nullptr,
            output_state
        };

        shared_ptr<ConnectionDetectingArguments> detectingArguments = std::make_shared<ConnectionDetectingArguments>(ConnectionDetectingArguments{
            eventLoop,
            client_fd,
            client_ip,
            client_port,
            connection_alived,
            false
        });
        
        eventLoop->init([](evutil_socket_t ev_fd, short flags, void * args){
            WritingEventWorkArguments* arguments = static_cast<WritingEventWorkArguments*>(args);

            char buf;
            bool stop_requested = false;
            // Read from pipe to clear it
            while (read(ev_fd, &buf, 1) == 1) {
                if (buf == 5) {
                    stop_requested = true;
                }
            }

            if (stop_requested || !arguments->connection_alived->load()) {
                warn_log("event loop receive stop notify for subscriber connection of {}:{}", arguments->client_ip, arguments->client_port);
                arguments->eventLoop->stop();
                return;
            }

            arguments->subscriber->processEventLoopOutput(arguments);
        }, eventArguments);
        eventLoop->setDisableDuplicateEntries(true);

        if (!eventLoop->initWriteEvent(
                client_fd,
                [](evutil_socket_t ev_fd, short flags, void * args) {
                    WritingEventWorkArguments* arguments = static_cast<WritingEventWorkArguments*>(args);
                    arguments->subscriber->processEventLoopOutput(arguments);
                },
                eventArguments)) {
            warn_log("fail to create subscriber socket write event for {}:{}", client_ip, client_port);
            connection_alived->store(false);
        }

        this->putConnectionEventLoop(client_ip, client_port, eventLoop);
        this->putConnectionDetectingArgs(client_ip, client_port, detectingArguments);

        thread event_thread([eventLoop, eventArguments, client_ip, client_port] {
            info_log("subscriber start run event loop for subscriber connection of {}:{}", client_ip, client_port);
            eventLoop->run();
            // this_thread::sleep_for(chrono::seconds(1));
            delete eventArguments;  // Clean up the heap-allocated arguments
            info_log("subscriber stop run event loop for subscriber connection of {}:{}", client_ip, client_port);
        });
        event_thread.detach();
    }

    void SubscriberBootstrap::disconnectEventLoopSubscriber(
        WritingEventWorkArguments *arguments,
        const string &reason) {

        this->removeSubscribed(arguments->client_ip, arguments->client_port);
        arguments->connection_alived->store(false);
        arguments->eventLoop->disableWriteEvent();
        shutdown(arguments->client_fd, SHUT_RDWR);
        warn_log("{} and remove subscribed for {}:{}",
            reason, arguments->client_ip, arguments->client_port);
        arguments->eventLoop->stop();
    }

    void SubscriberBootstrap::requeueTopicIfPending(
        WritingEventWorkArguments *arguments,
        const string &topic) {

        if (topic.empty() || arguments->consumeRecord == nullptr) {
            return;
        }
        optional<shared_ptr<repeater::MessageCircle>> circle =
            arguments->context.get_message_circle_composite()->getCircle(topic);
        if (!circle.has_value()) {
            return;
        }
        optional<repeater::ConsumeMeta> current_meta = arguments->consumeRecord->getMeta(topic);
        repeater::CircleMeta producer_meta = circle.value()->getMeta();
        if (current_meta.has_value() && current_meta->initialized &&
            current_meta->next_sequence < producer_meta.next_sequence) {
            arguments->eventLoop->submitWork(topic);
        }
    }

    void SubscriberBootstrap::processEventLoopOutput(WritingEventWorkArguments *arguments) {
        static constexpr int MAX_FRAMES_PER_CALLBACK = 16;

        if (!arguments->connection_alived->load()) {
            arguments->eventLoop->disableWriteEvent();
            return;
        }

        if (arguments->consumeRecord == nullptr &&
            this->isSubscribed(arguments->client_ip, arguments->client_port)) {
            optional<shared_ptr<repeater::ConsumeRecord>> record =
                arguments->context.get_consume_record_composite()->getRecord(
                    arguments->client_ip, arguments->client_port);
            if (record.has_value()) {
                arguments->consumeRecord = record.value();
            }
        }

        int completed_frames = 0;
        while (completed_frames < MAX_FRAMES_PER_CALLBACK) {
            string completed_topic;
            PendingWriteStatus write_status = this->flushPendingFrame(
                arguments->output_state,
                arguments->consumeRecord,
                arguments->client_fd,
                completed_topic);
            if (write_status == PendingWriteStatus::Error) {
                this->disconnectEventLoopSubscriber(arguments, "subscriber socket write failed");
                return;
            }
            if (write_status == PendingWriteStatus::WouldBlock) {
                if (!arguments->eventLoop->enableWriteEvent()) {
                    this->disconnectEventLoopSubscriber(arguments, "subscriber write event enable failed");
                }
                return;
            }
            if (write_status == PendingWriteStatus::Complete) {
                ++completed_frames;
                this->requeueTopicIfPending(arguments, completed_topic);
                continue;
            }

            if (!this->isSubscribed(arguments->client_ip, arguments->client_port)) {
                arguments->eventLoop->disableWriteEvent();
                return;
            }
            if (arguments->consumeRecord == nullptr) {
                arguments->eventLoop->disableWriteEvent();
                return;
            }

            string topic;
            if (!arguments->eventLoop->popWork(topic)) {
                arguments->eventLoop->disableWriteEvent();
                return;
            }
            optional<shared_ptr<repeater::MessageCircle>> circle =
                arguments->context.get_message_circle_composite()->getCircle(topic);
            if (!circle.has_value()) {
                continue;
            }

            TopicDeliveryStatus delivery_status = this->deliverTopic(
                arguments->config,
                arguments->consumeRecord,
                circle.value(),
                arguments->client_fd,
                topic,
                arguments->client_ip,
                arguments->client_port,
                arguments->output_state);
            if (delivery_status == TopicDeliveryStatus::Disconnect) {
                this->disconnectEventLoopSubscriber(arguments, "subscriber delivery failed");
                return;
            }
            if (delivery_status == TopicDeliveryStatus::WouldBlock) {
                if (!arguments->eventLoop->enableWriteEvent()) {
                    this->disconnectEventLoopSubscriber(arguments, "subscriber write event enable failed");
                }
                return;
            }
            if (delivery_status == TopicDeliveryStatus::Delivered) {
                ++completed_frames;
                this->requeueTopicIfPending(arguments, topic);
            }
        }

        arguments->eventLoop->disableWriteEvent();
        bool has_control_frames = false;
        {
            std::lock_guard<std::mutex> lock(arguments->output_state->control_mutex);
            has_control_frames = !arguments->output_state->control_frames.empty();
        }
        if (arguments->output_state->pending_frame.has_value() ||
            has_control_frames || arguments->eventLoop->hasWorks()) {
            if (!arguments->eventLoop->notifyStartWork()) {
                this->disconnectEventLoopSubscriber(arguments, "subscriber output requeue failed");
            }
        }
    }

    void SubscriberBootstrap::acceptHandle(repeater::RepeaterConfig &config, repeater::GlobalContext &context, int client_fd, string client_ip, int client_port) {

        shared_ptr<atomic_bool> connection_alived = std::make_shared<atomic_bool>(true);
        shared_ptr<SubscriberOutputState> output_state = std::make_shared<SubscriberOutputState>();
        if (config.subscriber_enable_event_loop) {
            this->startAcceptHandleEventLoopWritingThread(
                config, context, client_fd, client_ip, client_port, connection_alived, output_state);
        } else {
            this->startAcceptHandleNormalWritingThread(
                config, context, client_fd, client_ip, client_port, connection_alived, output_state);
        }

        thread reading_thread([this, client_fd, client_ip, client_port, &config, &context, connection_alived, output_state] {
            size_t HEADER_SIZE = 4;
            size_t MAX_MESSAGE_SIZE = 65536;

            // process ping and subscribe
            while (true) {
                if (!connection_alived->load()) {
                    break;
                }
                // Step1: read header of topic length
                uint32_t topic_length = 0;
                if (!receiveExact(
                        client_fd, &topic_length, HEADER_SIZE, connection_alived)) {
                    err_log("fail to read header of type from client of {}", this->role_);
                    break;
                }
                topic_length = ntohl(topic_length);

                #ifdef OPEN_STD_DEBUG_LOG
                    std::cout << this->role_ << " receive topic length: " << topic_length << std::endl;
                #endif

                // Step2: read header of topic name
                std::vector<char> topic_buffer(topic_length + 1);
                if (!receiveExact(
                        client_fd, topic_buffer.data(), topic_length, connection_alived)) {
                    err_log("fail to read complete topic from client of {}", this->role_);
                    break;
                }

                // Step3: read header of main data length
                uint32_t message_length = 0;
                if (!receiveExact(
                        client_fd, &message_length, HEADER_SIZE, connection_alived)) {
                    err_log("fail to read header of length from client of {}", this->role_);
                    break;
                }
                message_length = ntohl(message_length);
                #ifdef OPEN_STD_DEBUG_LOG
                    std::cout << this->role_ << " receive message length: " << message_length << std::endl;
                #endif

                if (message_length > MAX_MESSAGE_SIZE) {
                    err_log("message from client of {} is too large, which length is {}", this->role_, message_length);
                    break;
                }

                // Step4: read main data
                std::vector<char> message_buffer(message_length + 1);
                if (!receiveExact(
                        client_fd, message_buffer.data(), message_length, connection_alived)) {
                    err_log("fail to read complete message from client of {}", this->role_);
                    break;
                }

                message_buffer[message_length] = '\0';

                #ifdef OPEN_STD_DEBUG_LOG
                    std::cout << this->role_ << " receive data: " << topic_length << "," << topic_buffer.data() << "," << message_length << "," << message_buffer.data() << std::endl;
                #endif

                // Step5: process main data by message type
                if (topic_buffer.data() == connection::MESSAGE_OP_TOPIC_PING) {

                    if (!this->enqueueControlFrame(
                            output_state, connection::MESSAGE_OP_TOPIC_PONG, "ok")) {
                        break;
                    }
                    this->refreshKeepAlive(client_ip, client_port);

                } else if (topic_buffer.data() == connection::MESSAGE_OP_TOPIC_PONG) {
                    // Ingore this topic
                } else if (topic_buffer.data() == connection::MESSAGE_OP_TOPIC_SUBSCRIBE) {
                    string message_text = message_buffer.data();
                    
                    #ifdef OPEN_STD_DEBUG_LOG
                        std::cout << "recevie subscribe json from " << client_ip << ":" << client_port << " " << topic_buffer.data() << "," << message_text << std::endl;
                    #endif

                    vector<string> topics = this->parseSubscribeTopics(context, message_text);
                    if (topics.size() == 0) {
                        // topic not support
                        if (!this->enqueueControlFrame(
                                output_state,
                                connection::MESSAGE_OP_TOPIC_SUBSCRIBE,
                                "topic is empty or not support")) {
                            break;
                        }
                    } else {
                        if (context.get_consume_record_composite()->createNewRecord(client_ip, client_port, topics, config.max_topic_circle_size)) {
                            // success
                            optional<shared_ptr<repeater::ConsumeRecord>> record =
                                context.get_consume_record_composite()->getRecord(client_ip, client_port);
                            if (!record.has_value()) {
                                if (!this->enqueueControlFrame(
                                        output_state,
                                        connection::MESSAGE_OP_TOPIC_SUBSCRIBE,
                                        "fail subscribe")) {
                                    break;
                                }
                                continue;
                            }
                            for (string topic : topics) {
                                repeater::MessageSequence producer_sequence = 0;
                                optional<shared_ptr<repeater::MessageCircle>> circle =
                                    context.get_message_circle_composite()->getCircle(topic);
                                if (circle.has_value()) {
                                    producer_sequence = circle.value()->getMeta().next_sequence;
                                }
                                record.value()->initialize(topic, producer_sequence);
                            }

                            if (!this->enqueueControlFrame(
                                    output_state,
                                    connection::MESSAGE_OP_TOPIC_SUBSCRIBE,
                                    "ok")) {
                                break;
                            }
                            this->putSubscribed(client_ip, client_port);

                            for (string topic : topics) {
                                if (config.subscriber_enable_event_loop) {
                                    this->putTopicConnection(topic, client_ip, client_port);
                                    shared_ptr<repeater::EventLoopWorker> connection_event_loop;
                                    {
                                        std::shared_lock<std::shared_mutex> r_lock(this->rw_lock_);
                                        string key = client_ip + ":" + std::to_string(client_port);
                                        auto event_loop = this->connection_event_loop_map_.find(key);
                                        if (event_loop != this->connection_event_loop_map_.end()) {
                                            connection_event_loop = event_loop->second;
                                        }
                                    }
                                    if (connection_event_loop != nullptr && connection_event_loop->submitWork(topic)) {
                                        if (!connection_event_loop->notifyStartWork()) {
                                            warn_log("fail to initialize subscriber event loop for {}:{} topic={}",
                                                client_ip, client_port, topic);
                                        }
                                    }
                                }
                                info_log("client success to subscribe: client_ip={},client_port={},topic={}", client_ip, client_port, topic);
                            }
                        } else {
                            // failure
                            if (!this->enqueueControlFrame(
                                    output_state,
                                    connection::MESSAGE_OP_TOPIC_SUBSCRIBE,
                                    "fail subscribe")) {
                                break;
                            }
                        }
                    }
                } else {
                    // Ignore other topics
                }
            }

            connection_alived->store(false);
            shutdown(client_fd, SHUT_RDWR);
            close(client_fd);
            this->killAlive(client_ip, client_port);
        });
        reading_thread.detach();
    }

    void SubscriberBootstrap::clearConnectionResource(repeater::GlobalContext &context, string client_ip, int client_port) {
        context.get_consume_record_composite()->removeRecord(client_ip, client_port);
        this->removeSubscribed(client_ip, client_port);
        this->releaseConnectionEventData(client_ip, client_port);
    }

    vector<string> SubscriberBootstrap::parseSubscribeTopics(repeater::GlobalContext &context, string message_body) {

        set<string> topics;
        try {
            Json::Value json_result;
            Json::Reader reader;
            json_result.clear();
            reader.parse(message_body , json_result);
            // {"topics": ["T001","T002"]}
            if (json_result.isMember("topics") && json_result["topics"].isArray()) {
                for (Json::Value t : json_result["topics"]) {
                    if (context.is_allown_topic(t.asString()) && context.is_enabled_subscribe_topic(t.asString()) && !context.is_disabled_subscribe_topic(t.asString())) {
                        topics.insert(t.asString());
                    }
                }
            }
        } catch (std::exception &e) {
            err_log("exception occur while parse json: {}", e.what());
        }

        vector<string> result;
        for (string t : topics) {
            result.push_back(t);
        }

        return result;

    }

    void SubscriberBootstrap::putSubscribed(string client_ip, int client_port) {

        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        string key = client_ip + ":" + std::to_string(client_port);
        this->connection_subscribed_[key] = true;
    }

    void SubscriberBootstrap::removeSubscribed(string client_ip, int client_port) {

        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        string key = client_ip + ":" + std::to_string(client_port);
        this->connection_subscribed_.erase(key);
    }

    bool SubscriberBootstrap::isSubscribed(string client_ip, int client_port) {

        std::shared_lock<std::shared_mutex> w_lock(this->rw_lock_);
        string key = client_ip + ":" + std::to_string(client_port);
        auto result = this->connection_subscribed_.find(key);
        return result != this->connection_subscribed_.end() && result->second == true;
    }

    void SubscriberBootstrap::putConnectionEventLoop(string client_ip, int client_port, shared_ptr<repeater::EventLoopWorker> eventWork) {

        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        string key = client_ip + ":" + std::to_string(client_port);
        this->connection_event_loop_map_[key] = eventWork;
    }

    void SubscriberBootstrap::putConnectionDetectingArgs(string client_ip, int client_port, shared_ptr<ConnectionDetectingArguments> args) {

        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        string key = client_ip + ":" + std::to_string(client_port);
        this->connection_detecting_args_map_[key] = args;
    }

    void SubscriberBootstrap::putTopicConnection(string topic, string client_ip, int client_port) {

        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        string connection = client_ip + ":" + std::to_string(client_port);
        auto connections = this->topic_connection_map_.find(topic);
        if (connections != this->topic_connection_map_.end()) {
            connections->second.push_back(connection);
            this->topic_connection_map_[topic] = connections->second;
        } else {
            vector<string> values;
            values.push_back(connection);
            this->topic_connection_map_[topic] = values;
        }
    }

    void SubscriberBootstrap::releaseConnectionEventData(string client_ip, int client_port) {

        std::unique_lock<std::shared_mutex> w_lock(this->rw_lock_);
        string key = client_ip + ":" + std::to_string(client_port);

        auto eventArgs = this->connection_detecting_args_map_.find(key);
        if (eventArgs != this->connection_detecting_args_map_.end()) {
            eventArgs->second->connection_alived->store(false);
            shutdown(eventArgs->second->client_fd, SHUT_RDWR);
        }

        auto eventLoop = this->connection_event_loop_map_.find(key);
        if (eventLoop != this->connection_event_loop_map_.end()) {
            if (!eventLoop->second->notifyStopWork()) {
                warn_log("fail to notify event loop to stop while releasing subscriber {}:{}",
                    client_ip, client_port);
            }
            this->connection_event_loop_map_.erase(key);
        }
        int eventLoopCount = this->connection_event_loop_map_.size();

        if (eventArgs != this->connection_detecting_args_map_.end()) {
            this->connection_detecting_args_map_.erase(key);
        }
        int detectingArgsCount = this->connection_detecting_args_map_.size();

        vector<string> topics;
        for (auto [topic, _] : this->topic_connection_map_) {
            topics.push_back(topic);
        }
        for (string topic : topics) {
            auto connections = this->topic_connection_map_.find(topic);
            if (connections != this->topic_connection_map_.end()) {
                vector<string> remainConnctions;
                for (string connection : connections->second) {
                    if (connection != key) {
                        remainConnctions.push_back(connection);
                    }
                }
                this->topic_connection_map_[topic] = remainConnctions;
            }
        }
        info_log("subscriber release and remove event loop worker for {}:{}. remain {} event loop and {} detecting args", client_ip, client_port, eventLoopCount, detectingArgsCount);
    }
}
