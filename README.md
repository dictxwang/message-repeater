

# Message-Repeater

A high-performance, topic-based publish-subscribe message broker written in C++20. It acts as a central hub for distributing messages between publishers and subscribers via a binary TCP protocol, similar to RabbitMQ or Kafka but with a simpler, focused design.

## Key Features

- **Topic-based routing**: Messages are organized by topics with configurable access control (allowlist)
- **Binary TCP protocol**: 4-byte length-prefixed frames for efficient, low-overhead message transmission
- **Circular buffer storage**: Each topic uses a fixed-size circular buffer with wraparound tracking for temporary message storage
- **Event loop dispatch**: Optional libevent-based asynchronous message delivery for high-throughput subscriber scenarios
- **Multi-tier architecture**: Hierarchical deployment where repeaters can subscribe to other repeaters (Layer mode)
- **Connection management**: Heartbeat mechanism (ping/pong), idle detection, configurable connection limits, and send timeouts
- **High concurrency**: Multi-threaded design with `shared_mutex` for concurrent read access and exclusive write locks
- **Watchdog monitoring**: Background risk controller that monitors resource usage and sends alerts via Telegram bot
- **Multi-language clients**: C++, Python, and Go client examples included

## Architecture

```
                    ┌─────────────────────────────────────────┐
                    │            Message Repeater              │
                    │                                         │
  Publishers ──────►│  Publisher Service (TCP, e.g. :10001)   │
                    │         │                               │
                    │         ▼                               │
                    │  ┌─────────────────┐                    │
                    │  │  GlobalContext   │                    │
                    │  │  ┌────────────┐ │                    │
                    │  │  │ Message    │ │                    │
                    │  │  │ Circles    │ │  EventLoopWorker   │
                    │  │  │ (per topic)│ │──(libevent-based)──├────► Subscribers
                    │  │  └────────────┘ │                    │
                    │  │  ┌────────────┐ │                    │
                    │  │  │ Consume    │ │                    │
                    │  │  │ Records    │ │                    │
                    │  │  └────────────┘ │                    │
                    │  └─────────────────┘                    │
                    │                                         │
                    │  Subscriber Service (TCP, e.g. :20001)  │
                    │  Layer Service (optional, upstream sub)  │
                    │  Risk Controller (watchdog + TG alerts)  │
                    └─────────────────────────────────────────┘
                              ▲
                              │ Layer Subscribe
                    ┌─────────┴─────────┐
                    │  Upstream Repeater │
                    └───────────────────┘
```

### Core Services

1. **Publisher Service**: Accepts TCP connections from publishing clients, validates topics against the allowlist, and stores messages into per-topic circular buffers.

2. **Subscriber Service**: Delivers messages to subscribing clients. Supports two dispatch modes:
   - **Normal mode**: Synchronous per-connection threads
   - **Event loop mode**: libevent-based asynchronous dispatch for higher throughput

3. **Layer Service** (optional): Connects to an upstream repeater's subscriber port, relaying specified topics into the local instance. Enables multi-tier message relay networks.

4. **Risk Controller** (watchdog): A background thread that monitors circle buffer usage, connection counts, and resource limits every 2 minutes. Sends warning alerts via Telegram bot integration.

### Binary Protocol

```
Frame: [4-byte BE topic_length] [topic_name] [4-byte BE message_length] [message_body]

Reserved topics:
  "ping" / "pong"  — Heartbeat
  "subscribe"      — Subscription request
  "error"          — Error response
```

### Message Storage

Each topic has a `MessageCircle` — a fixed-size circular buffer that tracks:
- `overlapping_turns`: Number of times the buffer has wrapped around
- `index_offset`: Current write position

Subscribers maintain their own `ConsumeRecord` per topic, enabling reconnection and resume from the last consumed position.

## Tech Stack

| Component     | Technology                         |
|---------------|------------------------------------|
| Language      | C++20                              |
| Build system  | CMake 3.10+                        |
| Logging       | spdlog (async daily file logger)   |
| JSON          | jsoncpp                            |
| Event loop    | libevent                           |
| TLS/Crypto    | OpenSSL                            |
| HTTP client   | libcurl (for Telegram bot API)     |
| Networking    | POSIX TCP sockets (TCP_NODELAY)    |

## Build

### Install Dependencies

**Linux (Ubuntu)**:
```bash
sudo apt install pkg-config openssl libssl-dev libcurl4-openssl-dev libevent-dev
```

**macOS**:
```bash
brew install pkg-config openssl curl libevent
```

### Compile

```bash
mkdir -p build && cd build
cmake ..                        # Release mode (default)
# cmake -DUSE_DEBUG_MODE=ON ..  # Debug mode with OPEN_STD_DEBUG_LOG
make
```

Produces two executables:
- `build/repeater_starter` — Main repeater service
- `build/test_starter` — Test runner

## Configuration

The repeater is configured via a JSON file. See `config/repeater_layer.json` for a full example.

```json
{
    "logger_name": "Repeater",
    "logger_file_path": "logs/output.log",
    "logger_level": 2,
    "logger_max_files": 7,

    "process_node_name": "dev0001",
    "enable_run_watchdog": true,

    "tg_bot_token": "<your-telegram-bot-token>",
    "tg_chat_id": -123456789,
    "tg_send_message": true,

    "max_topic_number": 10,
    "max_topic_circle_size": 16,
    "max_message_body_size": 4096,
    "max_connection_idle_second": 30,

    "allown_topics": ["Sample0001", "Sample0002"],

    "socket_write_timeout_second": 5,

    "enable_layer_subscribe": false,
    "layer_subscribe_addresses": ["127.0.0.1:20001"],
    "layer_subscribe_topics": ["Sample0001"],

    "disable_accept_publisher": false,
    "publisher_listen_address": "127.0.0.1",
    "publisher_listen_port": 10001,
    "publisher_max_connection": 10,

    "subscriber_enable_event_loop": false,
    "subscriber_listen_address": "127.0.0.1",
    "subscriber_listen_port": 20001,
    "subscriber_max_connection": 20
}
```

### Key Configuration Options

| Key | Description |
|-----|-------------|
| `max_topic_number` | Maximum number of topics allowed |
| `max_topic_circle_size` | Circular buffer size per topic |
| `max_message_body_size` | Maximum message body size in bytes |
| `max_connection_idle_second` | Idle timeout before connection is closed |
| `allown_topics` | Allowlist of valid topic names |
| `subscriber_enable_event_loop` | Enable libevent-based async dispatch |
| `enable_layer_subscribe` | Enable multi-tier upstream subscription |
| `enable_run_watchdog` | Enable risk controller monitoring |
| `tg_send_message` | Enable Telegram alert notifications |

## Usage

```bash
./build/repeater_starter config/repeater_layer.json
```

## Client Examples

The project provides client examples in three languages under `examples/`:

### C++ (`examples/src/client/`)

- `RepeaterPublisher` / `RepeaterSubscriber` classes
- Binary protocol handling utilities
- Sample publisher and subscriber programs

### Python (`examples/python/`)

- `MessagePublisher` class with heartbeat thread
- JSON message support

### Go (`examples/go/message/`)

- TCP MQ publisher and subscriber implementations

## Project Structure

```
message-repeater/
├── src/
│   ├── repeater_starter.cpp        # Main entry point
│   ├── combiner/
│   │   ├── global_context.*        # Central state management
│   │   ├── message_container.*     # Circular buffer (MessageCircle)
│   │   ├── message_event.*         # EventLoopWorker (libevent)
│   │   └── risk_controller.*      # Watchdog + Telegram alerts
│   ├── connection/
│   │   └── acceptor.*             # Base TCP server, heartbeat
│   ├── publisher/
│   │   └── publisher_acceptor.*   # Publisher service
│   ├── subscriber/
│   │   └── subscriber_acceptor.*  # Subscriber service
│   ├── layer/
│   │   └── layer_connector.*      # Multi-tier connectivity
│   ├── config/                    # Configuration parsing
│   ├── json/                      # JSON utilities
│   ├── tgbot/                     # Telegram bot integration
│   ├── logger/                    # spdlog wrapper
│   └── util/                      # Common utilities
├── config/                        # Configuration files
├── examples/                      # Client examples (C++, Python, Go)
├── 3rdparty/spdlog/              # Bundled spdlog
├── CMakeLists.txt
└── dependencies.cmake
```

## Use Cases

- Real-time message distribution systems
- Event streaming between microservices
- IoT device communication hubs
- Multi-tier message relay networks
- Lightweight alternative to heavyweight message queue systems

## Dependencies Installation

### On Linux (Ubuntu)

```bash
sudo apt install pkg-config
sudo apt install openssl libssl-dev
sudo apt-get install libcurl4-openssl-dev
sudo apt install libevent-dev
```

### On macOS

```bash
brew install pkg-config
brew install openssl
brew install curl
brew install libevent
```
