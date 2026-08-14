#include "combiner/message_event.h"

#include <iostream>

int main() {
    repeater::EventLoopWorker worker;
    worker.setDisableDuplicateEntries(true);

    if (!worker.submitWork("A")) {
        std::cerr << "first dirty topic was not queued\n";
        return 1;
    }
    if (worker.submitWork("A")) {
        std::cerr << "duplicate dirty topic was queued\n";
        return 1;
    }
    if (!worker.submitWork("B")) {
        std::cerr << "second dirty topic was not queued\n";
        return 1;
    }

    std::string head;
    if (!worker.popWork(head) || head != "A") {
        std::cerr << "failed to pop one dirty topic\n";
        return 1;
    }
    if (!worker.hasWorks()) {
        std::cerr << "remaining dirty topic was lost\n";
        return 1;
    }
    if (!worker.submitWork("A")) {
        std::cerr << "popped topic could not be queued again\n";
        return 1;
    }

    const auto first = worker.popWorks();
    if (first.size() != 2 || first[0] != "B" || first[1] != "A") {
        std::cerr << "unexpected dirty topic batch\n";
        return 1;
    }

    if (!worker.submitWork("A")) {
        std::cerr << "processed topic could not be marked dirty again\n";
        return 1;
    }
    const auto second = worker.popWorks();
    if (second.size() != 1 || second[0] != "A") {
        std::cerr << "unexpected requeued topic batch\n";
        return 1;
    }

    std::cout << "all event queue tests passed\n";
    return 0;
}
