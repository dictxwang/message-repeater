# -*- coding: utf8 -*-
__author__ = 'dictwang'

import socket
import time
import threading

MessageTopic_Ping = "ping"
MessageTopic_Pong = "pong"
MessageTopic_Subscribe = "subscribe"

Pong_Success_Text = "ok"


class MessageSubscribe:
    def __init__(self, server_addr, server_port) -> None:
        self._server_addr = server_addr
        self._server_port = server_port
        self._client_fd = None
        self._connected = False

    def connect(self):
        self._create_connection()
        self._start_heartbeat()

    def subscribe(self, topics) -> bool:
        if not self._connected:
            return False

        topics = ",".join(list(map(lambda x : "\"" + x + "\"", topics)))
        body = "{\"topics\": [" + topics + "]}"

        send_data: bytes = int.to_bytes(4, length=4, byteorder="big")
        topic_data = MessageTopic_Ping.encode("utf-8")
        send_data += topic_data

        body_data = body.encode("utf-8")
        send_data += int.to_bytes(len(body_data), length=4, byteorder="big")
        send_data += body_data

        try:
            print("before send all")
            self._client_fd.sendall(send_data)
            print("after send all")

            topic_length_data = self._client_fd.recv(4)
            if len(topic_length_data) < 4:
                return False
            topic_length = int.from_bytes(topic_length_data, 'big')

            if topic_length < 1:
                return False

            topic_data = self._client_fd.recv(topic_length)
            if len(topic_data) <= 0:
                return False

            topic_name = topic_data.decode("utf-8")
            if topic_name != MessageTopic_Subscribe:
                return False

            body_length_data = self._client_fd.recv(4)
            if len(body_length_data) < 4:
                return False
            body_length = int.from_bytes(body_length_data, 'big')
            if body_length <= 0:
                return False

            body_data = self._client_fd.recv(body_length)
            if len(body_data) <= 0:
                return False
            text = body_data.decode("utf-8")
            if text.lower().find(Pong_Success_Text) >= 0:
                # success
                print(f"receive subscribe: {text}")
                return True
            else:
                return False
        except Exception as e:
            print(f"An error occurred: {e}")

        return False

    def read_message(self, topic_name, message_json) -> bool:
        if not self._connected:
            return False

        topic_name_data = topic_name.encode("utf-8")
        send_data: bytes = int.to_bytes(len(topic_name_data), length=4, byteorder="big")
        send_data += topic_name_data
        body_data = message_json.encode("utf-8")
        send_data += int.to_bytes(len(body_data), length=4, byteorder="big")
        send_data += body_data

        try:
            self._client_fd.sendall(send_data)
        except Exception as e:
            return False
        return True

    def _create_connection(self):
        self._client_fd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        try:
            # Connect to the server
            self._client_fd.connect((self._server_addr, self._server_port))
        except ConnectionRefusedError:
            print(f"Connection refused. Ensure the server is running on {self._server_addr}:{self._server_port}")
        except Exception as e:
            print(f"An error occurred: {e}")

        self._connected = True

    def _start_heartbeat(self):
        t = threading.Thread(target=self._send_ping_text, args=())
        t.start()

    def _send_ping_text(self):
        print(101)
        send_data: bytes = int.to_bytes(4, length=4, byteorder="big")
        topic_data = MessageTopic_Ping.encode("utf-8")
        send_data += topic_data

        body_data = "ok".encode("utf-8")
        send_data += int.to_bytes(len(body_data), length=4, byteorder="big")
        send_data += body_data

        print(102)

        while True:
            if not self._connected:
                break
            time.sleep(10)

            try:
                print("before send ping")
                self._client_fd.sendall(send_data)
                print("after send ping")

                topic_length_data = self._client_fd.recv(4)
                if len(topic_length_data) < 4:
                    break
                topic_length = int.from_bytes(topic_length_data, 'big')

                if topic_length < 1:
                    continue

                topic_data = self._client_fd.recv(topic_length)
                if len(topic_data) <= 0:
                    break

                topic_name = topic_data.decode("utf-8")
                if topic_name != MessageTopic_Pong:
                    continue

                body_length_data = self._client_fd.recv(4)
                if len(body_length_data) < 4:
                    break
                body_length = int.from_bytes(body_length_data, 'big')
                if body_length <= 0:
                    continue

                body_data = self._client_fd.recv(body_length)
                if len(body_data) <= 0:
                    break
                text = body_data.decode("utf-8")
                if text.lower().find(Pong_Success_Text) >= 0:
                    # success
                    print(f"receive pong: {text}")
                    pass
                else:
                    # fail
                    break

            except Exception as e:
                print(f"An error occurred: {e}")
                break

        self._client_fd.close()
        self._connected = False
        print("tcp connection closed")


if __name__ == "__main__":
    subscriber = MessageSubscribe("127.0.0.1", 20001)
    subscriber.connect()

    # # subscribe
    # time.sleep(5)
    # topics = ["T001", "T002"]
    # result = subscriber.subscribe(topics)

    while True:
        pass
