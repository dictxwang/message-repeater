#include "subscriber_acceptor.h"

using namespace std;

namespace subscriber {

    void SubscriberBootstrap::startMessageDispatch(repeater::GlobalContext &context) {
        
        thread dispatch_thread([this, &context] {

            while (true) {
                this_thread::sleep_for(chrono::microseconds(10));
                auto topics = context.pop_message_topics();
                if (topics.size() == 0) {
                    continue;
                }
                for (string topic : topics) {
                    this->dispatchMessage(topic);
                }
            }
        });
        dispatch_thread.detach();
        info_log("subscriber start message dispatch thread");
    }

    void SubscriberBootstrap::dispatchMessage(string topic) {
        std::shared_lock<std::shared_mutex> w_lock(this->rw_lock_);
        std::cout << "<<<<< 001" << std::endl;
        auto connections = this->topic_connection_map_.find(topic);
        if (connections == this->topic_connection_map_.end()) {
            return;
        }
        std::cout << "<<<<< 002" << std::endl;

        for (string connection : connections->second) {
            auto event_loop = this->connection_event_loop_map_.find(connection);
            std::cout << "<<<<< 003" << std::endl;
            if (event_loop != this->connection_event_loop_map_.end()) {
                std::cout << "<<<<< 004" << std::endl;
                event_loop->second->submitWork(topic);
                std::cout << "<<<<< 005" << std::endl;
                if (event_loop->second->getWorkEvent() != nullptr) {
                    std::cout << "work event is not null" << std::endl;
                }
                evuser_trigger(event_loop->second->getWorkEvent());
                std::cout << "<<<<< 006" << std::endl;
                info_log("[debug] submit work of {} to {}", topic, connection);
            }
        }
    }

    void SubscriberBootstrap::acceptHandle(repeater::RepeaterConfig &config, repeater::GlobalContext &context, int client_fd, string client_ip, int client_port) {

        shared_ptr<bool> connection_alived = std::make_shared<bool>(true);
        unordered_map<string, bool> firstReadCircle;
        shared_ptr<repeater::EventLoopWorker> eventLoop = std::make_shared<repeater::EventLoopWorker>();

        EventWorkArguments arguments = {
            eventLoop,
            this,
            client_fd,
            client_ip,
            client_port,
            config,
            context,
            connection_alived,
            firstReadCircle,
        };
        
        eventLoop->init([](evutil_socket_t ev_fd, short flags, void * args){
            EventWorkArguments* arguments = static_cast<EventWorkArguments*>(args);

            vector<string> topics = arguments->eventLoop->popWorks();
            if (topics.size() == 0) {
                return;
            }

            optional<shared_ptr<repeater::ConsumeRecord>> record = arguments->context.get_consume_record_composite()->getRecord(arguments->client_ip, arguments->client_port);
            if (!record.has_value()) {
                return;
            }

            for (string topic : record.value()->getTopics()) {

                optional<repeater::CircleMeta> meta = record.value()->getMeta(topic);
                if (meta.has_value()) {

                    bool firstReadCircle = false;
                    if (arguments->firstReadCircle.find(topic) == arguments->firstReadCircle.end()) {
                        firstReadCircle = true;
                        arguments->firstReadCircle[topic] = true;
                    }
                    optional<shared_ptr<repeater::MessageCircle>> circle = arguments->context.get_message_circle_composite()->getCircle(topic);
                    if (circle.has_value()) {
                        tuple<optional<string>, int, int> message_result = circle.value()->getMessageAndCircleMeta(meta->overlapping_turns, meta->index_offset, firstReadCircle);
                        auto message = std::get<0>(message_result);
                        if (message.has_value()) {

                            if (message.value().size() > 0) {
                                // write message to client
                                if (arguments->subscriber->sendSocketData(arguments->client_fd, topic, message.value())) {
                                    // write fail, remove subscribed
                                    arguments->subscriber->removeSubscribed(arguments->client_ip, arguments->client_port);

                                    warn_log("subscriber write fail and remove subscribed for {}:{}", arguments->client_ip, arguments->client_port);
                                    break;
                                }
                            }

                            // update record
                            int producer_overlapping = std::get<1>(message_result);
                            int producer_index_offset = std::get<2>(message_result);
                            record.value()->updateMeta(topic, producer_overlapping, producer_index_offset);
                        }
                    }
                }
            }
        }, &arguments);

        this->putConnectionEventLoop(client_ip, client_port, eventLoop);

        thread write_thread([&eventLoop, arguments] {
            eventLoop->run();
            info_log("[debug] start event loop run for {}:{}", arguments.client_ip, arguments.client_port);

            while (true) {
                this_thread::sleep_for(chrono::milliseconds(1));
                if (!(*arguments.connection_alived)) {
                    break;
                }
                
                if (!arguments.subscriber->isConnectionExists(arguments.client_ip, arguments.client_port)) {
                    info_log("subscriber connection not exists for {}:{}", arguments.client_ip, arguments.client_port);
                    break;
                }

                if (!arguments.subscriber->isSubscribed(arguments.client_ip, arguments.client_port)) {
                    continue;
                }

                optional<shared_ptr<repeater::ConsumeRecord>> record = arguments.context.get_consume_record_composite()->getRecord(arguments.client_ip, arguments.client_port);
                if (!record.has_value()) {
                    break;
                }
            }
            close(arguments.client_fd);
            arguments.subscriber->killAlive(arguments.client_ip, arguments.client_port);
            (*arguments.connection_alived) = false;
        });
        write_thread.detach();

        // thread write_thread([this, client_fd, client_ip, client_port, &config, &context, connection_alived] {
        //     bool firstReadCircle = true;
        //     while (true) {
        //         this_thread::sleep_for(chrono::microseconds(100));
        //         if (!(*connection_alived)) {
        //             break;
        //         }
        //         if (!this->isConnectionExists(client_ip, client_port)) {
        //             info_log("subscriber connection not exists for {}:{}", client_ip, client_port);
        //             break;
        //         }
        //         if (!this->isSubscribed(client_ip, client_port)) {
        //             continue;
        //         }

        //         optional<shared_ptr<repeater::ConsumeRecord>> record = context.get_consume_record_composite()->getRecord(client_ip, client_port);
        //         if (!record.has_value()) {
        //             break;
        //         }

        //         for (string topic : record.value()->getTopics()) {

        //             optional<repeater::CircleMeta> meta = record.value()->getMeta(topic);
        //             if (meta.has_value()) {

        //                 optional<shared_ptr<repeater::MessageCircle>> circle = context.get_message_circle_composite()->getCircle(topic);
        //                 if (circle.has_value()) {
        //                     tuple<optional<string>, int, int> message_result = circle.value()->getMessageAndCircleMeta(meta->overlapping_turns, meta->index_offset, firstReadCircle);
        //                     auto message = std::get<0>(message_result);
        //                     if (message.has_value()) {

        //                         if (message.value().size() > 0) {
        //                             // write message to client
        //                             if (!this->sendSocketData(client_fd, topic, message.value())) {
        //                                 // write fail, remove subscribed
        //                                 this->removeSubscribed(client_ip, client_port);

        //                                 warn_log("subscriber write fail and remove subscribed for {}:{}", client_ip, client_port);
        //                                 break;
        //                             }
        //                         }

        //                         // update record
        //                         int producer_overlapping = std::get<1>(message_result);
        //                         int producer_index_offset = std::get<2>(message_result);
        //                         record.value()->updateMeta(topic, producer_overlapping, producer_index_offset);
        //                     }
        //                 }
        //             }
        //         }
        //         firstReadCircle = false;
        //     }
        //     close(client_fd);
        //     this->killAlive(client_ip, client_port);
        //     (*connection_alived) = false;
        // });
        // write_thread.detach();

        thread read_thread([this, client_fd, client_ip, client_port, &config, &context, connection_alived] {
            size_t HEADER_SIZE = 4;
            size_t MAX_MESSAGE_SIZE = 65536;

            // process ping and subscribe
            while (true) {
                if (!(*connection_alived)) {
                    break;
                }
                // Step1: read header of topic length
                uint32_t topic_length = 0;
                ssize_t bytes_received = recv(client_fd, &topic_length, HEADER_SIZE, MSG_WAITALL);
                
                if (bytes_received != HEADER_SIZE) {
                    if (bytes_received == 0) {
                        err_log("client of {} disconnected", this->role_);
                    } else {
                        err_log("fail to read header of type from client of {}", this->role_);
                    }
                    break;
                }
                topic_length = ntohl(topic_length);

                #ifdef OPEN_STD_DEBUG_LOG
                    std::cout << this->role_ << " receive topic length: " << topic_length << std::endl;
                #endif

                // Step2: read header of topic name
                std::vector<char> topic_buffer(topic_length + 1);
                bytes_received = recv(client_fd, topic_buffer.data(), topic_length, MSG_WAITALL);
                if (bytes_received != topic_length) {
                    err_log("fail to read complete topic from client of {}", this->role_);
                    break;
                }

                // Step3: read header of main data length
                uint32_t message_length = 0;
                bytes_received = recv(client_fd, &message_length, HEADER_SIZE, MSG_WAITALL);
                if (bytes_received != HEADER_SIZE) {
                    if (bytes_received == 0) {
                        err_log("client of {} disconnected", this->role_);
                    } else {
                        err_log("fail to read header of length from client of {}", this->role_);
                    }
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
                bytes_received = recv(client_fd, message_buffer.data(), message_length, MSG_WAITALL);
                
                if (bytes_received != message_length) {
                    err_log("fail to read complete message from client of {}", this->role_);
                    break;
                }

                message_buffer[message_length] = '\0';

                #ifdef OPEN_STD_DEBUG_LOG
                    std::cout << this->role_ << " receive data: " << topic_length << "," << topic_buffer.data() << "," << message_length << "," << message_buffer.data() << std::endl;
                #endif

                // Step5: process main data by message type
                if (topic_buffer.data() == connection::MESSAGE_OP_TOPIC_PING) {

                    if (this->sendSocketData(client_fd, connection::MESSAGE_OP_TOPIC_PONG, "ok")) {
                        this->refreshKeepAlive(client_ip, client_port);
                    }

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
                        this->sendSocketData(client_fd, connection::MESSAGE_OP_TOPIC_SUBSCRIBE, "topic is empty or not support");
                    } else {
                        if (context.get_consume_record_composite()->createNewRecord(client_ip, client_port, topics, config.max_topic_circle_size)) {
                            // success
                            if (this->sendSocketData(client_fd, connection::MESSAGE_OP_TOPIC_SUBSCRIBE, "ok")) {
                                this->putSubscribed(client_ip, client_port);
                                for (string topic : topics) {
                                    this->putTopicConnection(topic, client_ip, client_port);
                                }
                                info_log("client success to subscribe: client_ip={},client_port={}", client_ip, client_port);
                            }
                        } else {
                            // failure
                            this->sendSocketData(client_fd, connection::MESSAGE_OP_TOPIC_SUBSCRIBE, "fail subscribe");
                        }
                    }
                } else {
                    // Ignore other topics
                }
            }

            close(client_fd);
            this->killAlive(client_ip, client_port);
            (*connection_alived) = false;
        });
        read_thread.detach();
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
                    if (context.is_allown_topic(t.asString())) {
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
        auto eventLoop = this->connection_event_loop_map_.find(key);
        if (eventLoop != this->connection_event_loop_map_.end()) {
            eventLoop->second->stop();
            this->connection_event_loop_map_.erase(key);
            info_log("[debug] stop and remove event loop for {}:{}", client_ip, client_port);
        }

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
        info_log("subscriber release and remove event loop for {}:{}", client_ip, client_port);
    }
}