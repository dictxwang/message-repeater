

# Message-Repeater

Project Summary

Message Repeater is a high-performance message broker system written in C++20 that implements a topic-based publish-subscribe pattern. It acts as a central hub for distributing messages between publishers and subscribers, similar to message queue systems like RabbitMQ or Kafka but with a simpler, focused design.

Key Features:

- Topic-based routing: Messages are organized by topics, allowing selective subscription
- Binary TCP protocol: Efficient message transmission with minimal overhead
- Circular buffer storage: Each topic uses a circular buffer for temporary message storage
- Multi-tier architecture: Supports hierarchical deployment where repeaters can subscribe to other repeaters
- Connection management: Built-in heartbeat mechanism, idle detection, and connection limits
- High concurrency: Multi-threaded design with shared mutex for concurrent access

Architecture:

The system consists of three main services:
1. Publisher Service (like port 10001): Receives messages from publishing clients
2. Subscriber Service (like port 20001): Delivers messages to subscribing clients
3. Layer Service (optional): Enables repeater-to-repeater connections for scaling

Use Cases:

- Real-time message distribution systems
- Event streaming between microservices
- IoT device communication hubs
- Multi-tier message relay networks
- Simple alternative to heavyweight message queue systems

Technical Stack:

C++20, TCP sockets, spdlog logging, CMake build system, JSON for configuration and payloads

The project provides Python client examples and C++ client examples demonstrating how to publish and subscribe to messages, making it easy to integrate with various applications.