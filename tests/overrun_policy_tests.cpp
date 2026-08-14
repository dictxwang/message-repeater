#include "combiner/message_container.h"

#include <iostream>
#include <string>

namespace {

int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << __FILE__ << ':' << __LINE__ << " CHECK failed: " #condition << '\n'; \
        ++failures; \
    } \
} while (false)

void fill_overrun_circle(repeater::MessageCircle& circle) {
    circle.append("zero");
    circle.append("one");
    circle.append("two");
    circle.append("three");
}

void test_sequence_and_sequential_read() {
    repeater::MessageCircle circle("T", 3);
    circle.append("zero");
    circle.append("one");
    circle.append("two");

    const auto meta = circle.getMeta();
    CHECK(meta.next_sequence == 3);
    CHECK(meta.oldest_available_sequence == 0);

    const auto result = circle.read(1, false, repeater::SubscriberOverrunPolicy::Latest);
    CHECK(result.status == repeater::MessageReadStatus::Message);
    CHECK(result.message.has_value() && result.message.value() == "one");
    CHECK(result.message_sequence == 1);
    CHECK(result.next_sequence == 2);
    CHECK(!result.overrun);
}

void test_latest_overrun_policy() {
    repeater::MessageCircle circle("T", 3);
    fill_overrun_circle(circle);
    const auto result = circle.read(0, false, repeater::SubscriberOverrunPolicy::Latest);

    CHECK(result.status == repeater::MessageReadStatus::Message);
    CHECK(result.overrun);
    CHECK(result.message.has_value() && result.message.value() == "three");
    CHECK(result.message_sequence == 3);
    CHECK(result.next_sequence == 4);
    CHECK(result.oldest_available_sequence == 1);
    CHECK(result.skipped_messages == 3);
}

void test_oldest_overrun_policy() {
    repeater::MessageCircle circle("T", 3);
    fill_overrun_circle(circle);
    const auto result = circle.read(0, false, repeater::SubscriberOverrunPolicy::Oldest);

    CHECK(result.status == repeater::MessageReadStatus::Message);
    CHECK(result.overrun);
    CHECK(result.message.has_value() && result.message.value() == "one");
    CHECK(result.message_sequence == 1);
    CHECK(result.next_sequence == 2);
    CHECK(result.skipped_messages == 1);
}

void test_disconnect_overrun_policy() {
    repeater::MessageCircle circle("T", 3);
    fill_overrun_circle(circle);
    const auto result = circle.read(0, false, repeater::SubscriberOverrunPolicy::Disconnect);

    CHECK(result.status == repeater::MessageReadStatus::Disconnect);
    CHECK(result.overrun);
    CHECK(!result.message.has_value());
    CHECK(result.next_sequence == 0);
    CHECK(result.skipped_messages == 1);
}

void test_latest_only_takes_precedence_over_overrun_policy() {
    repeater::MessageCircle circle("T", 3);
    fill_overrun_circle(circle);
    const auto result = circle.read(0, true, repeater::SubscriberOverrunPolicy::Disconnect);

    CHECK(result.status == repeater::MessageReadStatus::Message);
    CHECK(result.overrun);
    CHECK(result.message.has_value() && result.message.value() == "three");
    CHECK(result.next_sequence == 4);
}

void test_consumer_initialization_and_advance() {
    repeater::ConsumeRecord record("127.0.0.1", 10000, {"T"}, 3);
    auto meta = record.getMeta("T");
    CHECK(meta.has_value() && !meta->initialized);

    record.initialize("T", 7);
    meta = record.getMeta("T");
    CHECK(meta.has_value() && meta->initialized && meta->next_sequence == 7);

    record.initialize("T", 9);
    meta = record.getMeta("T");
    CHECK(meta.has_value() && meta->next_sequence == 7);

    record.updateSequence("T", 8);
    meta = record.getMeta("T");
    CHECK(meta.has_value() && meta->next_sequence == 8);
}

}  // namespace

int main() {
    test_sequence_and_sequential_read();
    test_latest_overrun_policy();
    test_oldest_overrun_policy();
    test_disconnect_overrun_policy();
    test_latest_only_takes_precedence_over_overrun_policy();
    test_consumer_initialization_and_advance();

    if (failures == 0) {
        std::cout << "all overrun policy tests passed\n";
        return 0;
    }
    std::cerr << failures << " overrun policy test(s) failed\n";
    return 1;
}
