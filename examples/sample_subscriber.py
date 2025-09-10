# -*- coding: utf8 -*-
__author__ = 'dictwang'

import socket
import time
import threading

MessageTopic_Ping = "ping"
MessageTopic_Pong = "pong"
MessageTopic_Subscribe = "subscribe"

Message_Success_Text = "ok"


class MessageSubscribe:
    def __init__(self, server_addr, server_port) -> None:
        self._server_addr = server_addr
        self._server_port = server_port
        self._client_fd = None
        self._connected = False
        self._subscribed = False

    def connect(self):
        self._create_connection()
        self._start_heartbeat()

    def close(self):
        if self._client_fd:
            try:
                self._client_fd.close()
            except Exception as exp:
                pass
        self._client_fd = None
        self._connected = False
        self._subscribed = False

    def subscribe(self, topics) -> bool:
        if not self._connected:
            return False

        topics = ",".join(list(map(lambda x : "\"" + x + "\"", topics)))
        body = "{\"topics\": [" + topics + "]}"

        send_data: bytes = int.to_bytes(len(MessageTopic_Subscribe), length=4, byteorder="big")
        topic_data = MessageTopic_Subscribe.encode("utf-8")
        send_data += topic_data

        body_data = body.encode("utf-8")
        send_data += int.to_bytes(len(body_data), length=4, byteorder="big")
        send_data += body_data

        try:
            self._client_fd.sendall(send_data)

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
            if text.lower().find(Message_Success_Text) >= 0:
                # success
                print(f"subscribe success and message is : {text}")
                self._subscribed = True
                return True
            else:
                print(f"subscribe failure and message is : {text}")
                return False
        except Exception as e:
            print(f"An error occurred when subscribe: {e}")

        return False

    def read_message(self) -> (bool, bool, str, str):
        if not self._connected:
            return False, False, "", ""

        topic_length_data = self._client_fd.recv(4)
        if len(topic_length_data) < 4:
            return False, False, "", ""
        topic_length = int.from_bytes(topic_length_data, 'big')

        if topic_length < 1:
            return False, False, "", ""

        topic_data = self._client_fd.recv(topic_length)
        if len(topic_data) <= 0:
            return False, False, "", ""

        topic_name = topic_data.decode("utf-8")

        body_length_data = self._client_fd.recv(4)
        if len(body_length_data) < 4:
            return False, False, "", ""
        body_length = int.from_bytes(body_length_data, 'big')
        if body_length <= 0:
            return False, False, "", ""

        body_data = self._client_fd.recv(body_length)
        if len(body_data) <= 0:
            return False, False, "", ""
        text = body_data.decode("utf-8")
        return True, True, topic_name, text

    def _create_connection(self):
        self._client_fd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        try:
            # Connect to the server
            self._client_fd.connect((self._server_addr, self._server_port))
        except ConnectionRefusedError:
            print(f"Connection refused. Ensure the server is running on {self._server_addr}:{self._server_port}")
        except Exception as e:
            print(f"An error occurred when create connection: {e}")

        self._connected = True

    def _start_heartbeat(self):
        t = threading.Thread(target=self._send_ping_text, args=())
        t.start()

    def _send_ping_text(self):
        send_data: bytes = int.to_bytes(4, length=4, byteorder="big")
        topic_data = MessageTopic_Ping.encode("utf-8")
        send_data += topic_data

        body_data = "ok".encode("utf-8")
        send_data += int.to_bytes(len(body_data), length=4, byteorder="big")
        send_data += body_data

        while True:
            is_disconnected = False
            # sleep 10 seconds and determine whether the socket is closed quickly
            for _ in range(0, 20):
                time.sleep(0.5)
                if not self._connected:
                    is_disconnected = True
                    break
            
            if is_disconnected:
                break

            try:
                self._client_fd.sendall(send_data)
            except Exception as e:
                print(f"An error occurred when send ping: {e}")
                self.close()
                break
        print("ping thread exited")


if __name__ == "__main__":

    subscriber = MessageSubscribe("127.0.0.1", 20001)
    topics = ["Sample0001", "Sample0002"]

    while True:
        # wait while for retry
        time.sleep(2)
        subscriber.connect()

        # before subscribe, wait for connection is ready
        time.sleep(2)
        result = subscriber.subscribe(topics)
        if not result:
            subscriber.close()
            print("not subscribe")
            continue

        time.sleep(1)
        while True:
            connected, subscribed, topic_name, message = subscriber.read_message()
            if not connected or not subscribed:
                subscriber.close()
                break
            print(f"read topic: {topic_name}, message: {message}")
