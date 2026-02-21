package message

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"net"
	"sync"
	"time"
)

type TcpMessagePublisher struct {
	serverAddr string
	serverPort int
	clientConn net.Conn
	connected  bool
	mu         sync.Mutex
	closeOnce  sync.Once
	closeCh    chan struct{}
}

func (mp *TcpMessagePublisher) Init(serverAddr string, serverPort int) {
	mp.serverAddr = serverAddr
	mp.serverPort = serverPort
	mp.closeCh = make(chan struct{})
}

func (mp *TcpMessagePublisher) Connect() error {
	if err := mp.createConnection(); err != nil {
		return err
	}
	mp.startHeartbeat()
	return nil
}

func (mp *TcpMessagePublisher) Close() {
	mp.closeOnce.Do(func() {
		mp.mu.Lock()
		defer mp.mu.Unlock()
		close(mp.closeCh)

		if mp.clientConn != nil {
			mp.clientConn.Close()
		}
		mp.clientConn = nil
		mp.connected = false
	})
}

func (mp *TcpMessagePublisher) Publish(topic string, message string) error {
	mp.mu.Lock()
	if !mp.connected {
		mp.mu.Unlock()
		return errors.New("not connected")
	}
	conn := mp.clientConn
	mp.mu.Unlock()

	msgBytes := mp.buildMessage(topic, message)

	if err := conn.SetWriteDeadline(time.Now().Add(time.Second * 3)); err != nil {
		return errors.New("failed to set write deadline")
	}

	if _, err := conn.Write(msgBytes); err != nil {
		mp.Close()
		return fmt.Errorf("failed to write message: %v", err)
	}

	return nil
}

func (mp *TcpMessagePublisher) IsConnected() bool {
	mp.mu.Lock()
	defer mp.mu.Unlock()
	return mp.connected
}

func (mp *TcpMessagePublisher) buildMessage(topic string, body string) []byte {
	topicBytes := []byte(topic)
	bodyBytes := []byte(body)

	topicLengthBytes := make([]byte, 4)
	binary.BigEndian.PutUint32(topicLengthBytes, uint32(len(topicBytes)))

	bodyLengthBytes := make([]byte, 4)
	binary.BigEndian.PutUint32(bodyLengthBytes, uint32(len(bodyBytes)))

	var buffer bytes.Buffer
	buffer.Write(topicLengthBytes)
	buffer.Write(topicBytes)
	buffer.Write(bodyLengthBytes)
	buffer.Write(bodyBytes)

	return buffer.Bytes()
}

func (mp *TcpMessagePublisher) createConnection() error {
	conn, err := net.Dial("tcp", fmt.Sprintf("%s:%d", mp.serverAddr, mp.serverPort))
	if err != nil {
		return fmt.Errorf("connection refused. Ensure the server is running on %s:%d: %v",
			mp.serverAddr, mp.serverPort, err)
	}

	mp.mu.Lock()
	mp.clientConn = conn
	mp.connected = true
	mp.mu.Unlock()

	return nil
}

func (mp *TcpMessagePublisher) startHeartbeat() {
	go mp.sendPingText()
}

func (mp *TcpMessagePublisher) sendPingText() {
	// Prepare ping message
	topicBytes := []byte(MessageTopicPing)
	bodyBytes := []byte("ok")

	topicLengthBytes := make([]byte, 4)
	binary.BigEndian.PutUint32(topicLengthBytes, uint32(len(topicBytes)))

	bodyLengthBytes := make([]byte, 4)
	binary.BigEndian.PutUint32(bodyLengthBytes, uint32(len(bodyBytes)))

	ticker := time.NewTicker(10 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-mp.closeCh:
			return
		case <-ticker.C:
			mp.mu.Lock()
			if !mp.connected || mp.clientConn == nil {
				mp.mu.Unlock()
				return
			}
			conn := mp.clientConn
			deadErr := conn.SetWriteDeadline(time.Now().Add(time.Second * 3))
			mp.mu.Unlock()

			if deadErr != nil {
				mp.Close()
				return
			}

			var buffer bytes.Buffer
			buffer.Write(topicLengthBytes)
			buffer.Write(topicBytes)
			buffer.Write(bodyLengthBytes)
			buffer.Write(bodyBytes)

			pingBytes := buffer.Bytes()

			if _, err := conn.Write(pingBytes); err != nil {
				mp.Close()
				return
			}
		}
	}
}
