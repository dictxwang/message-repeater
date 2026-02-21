package message

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"net"
	"strings"
	"sync"
	"time"
)

const (
	MessageTopicPing      = "ping"
	MessageTopicPong      = "pong"
	MessageTopicSubscribe = "subscribe"
	MessageSuccessText    = "ok"
)

type TcpMessageSubscribe struct {
	serverAddr string
	serverPort int
	clientConn net.Conn
	connected  bool
	subscribed bool
	mu         sync.Mutex
	closeOnce  sync.Once
	closeCh    chan struct{}
}

func (ms *TcpMessageSubscribe) Init(serverAddr string, serverPort int) {
	ms.serverAddr = serverAddr
	ms.serverPort = serverPort
	ms.closeCh = make(chan struct{})
}

func (m *TcpMessageSubscribe) Connect() error {
	if err := m.createConnection(); err != nil {
		return err
	}
	m.startHeartbeat()
	return nil
}

func (m *TcpMessageSubscribe) Close() {
	m.closeOnce.Do(func() {
		m.mu.Lock()
		defer m.mu.Unlock()
		close(m.closeCh)

		if m.clientConn != nil {
			m.clientConn.Close()
		}
		m.clientConn = nil
		m.connected = false
		m.subscribed = false
	})
}

func (m *TcpMessageSubscribe) Subscribe(topics []string) error {
	m.mu.Lock()
	if !m.connected {
		m.mu.Unlock()
		return errors.New("not connected")
	}
	conn := m.clientConn
	m.mu.Unlock()

	// Build JSON body
	quotedTopics := make([]string, len(topics))
	for i, topic := range topics {
		quotedTopics[i] = fmt.Sprintf(`"%s"`, topic)
	}
	body := fmt.Sprintf(`{"topics": [%s]}`, strings.Join(quotedTopics, ","))

	// Build message
	topicBytes := []byte(MessageTopicSubscribe)
	bodyBytes := []byte(body)
	topicLengthBytes := make([]byte, 4)
	binary.BigEndian.PutUint32(topicLengthBytes, uint32(len(topicBytes)))
	bodyLengthBytes := make([]byte, 4)
	binary.BigEndian.PutUint32(bodyLengthBytes, uint32(len(bodyBytes)))

	writeDeadErr := m.clientConn.SetWriteDeadline(time.Now().Add(time.Second * 3))
	readDeadErr := m.clientConn.SetReadDeadline(time.Now().Add(time.Second * 3))
	if writeDeadErr != nil {
		return errors.New("fail to set write deadline")
	}
	if readDeadErr != nil {
		return errors.New("fail to set read deadline")
	}

	var buffer bytes.Buffer
	buffer.Write(topicLengthBytes)
	buffer.Write(topicBytes)
	buffer.Write(bodyLengthBytes)
	buffer.Write(bodyBytes)

	subscribeBytes := buffer.Bytes()
	if _, err := conn.Write(subscribeBytes); err != nil {
		return fmt.Errorf("fail to wrtie subscribe data: %+v", err)
	}

	// Read response
	// Read topic length
	var topicLen uint32
	if err := binary.Read(conn, binary.BigEndian, &topicLen); err != nil {
		return errors.New("fail to read topic length")
	}
	if topicLen < 1 {
		return errors.New("invalid topic length")
	}

	// Read topic
	topicBuf := make([]byte, topicLen)
	if _, err := conn.Read(topicBuf); err != nil {
		errors.New("fail to read topic data")
	}
	responseTopic := string(topicBuf)

	if responseTopic != MessageTopicSubscribe {
		return errors.New("invalid subscribe topic")
	}

	// Read body length
	var bodyLen uint32
	if err := binary.Read(conn, binary.BigEndian, &bodyLen); err != nil {
		return errors.New("fail to read body length")
	}
	if bodyLen <= 0 {
		errors.New("invalid body length")
	}

	// Read body
	bodyBuf := make([]byte, bodyLen)
	if _, err := conn.Read(bodyBuf); err != nil {
		errors.New("fail to read body data")
	}
	responseBody := string(bodyBuf)

	if strings.Contains(strings.ToLower(responseBody), MessageSuccessText) {
		m.mu.Lock()
		m.subscribed = true
		m.mu.Unlock()
		return nil
	}

	return errors.New("response not success")
}

func (m *TcpMessageSubscribe) ReadMessage(timeoutDuration time.Duration) (connected bool, subscribed bool, topic string, message string) {
	m.mu.Lock()
	if !m.connected {
		m.mu.Unlock()
		return false, false, "", ""
	}
	conn := m.clientConn
	isSubscribed := m.subscribed
	deadErr := conn.SetReadDeadline(time.Now().Add(timeoutDuration))
	m.mu.Unlock()

	if deadErr != nil {
		m.Close()
		return false, false, "", ""
	}

	// Read topic length
	var topicLen uint32
	topicLenBuf := make([]byte, 4)
	if readLen, err := conn.Read(topicLenBuf); err != nil || readLen != 4 {
		return false, false, "", ""
	}
	topicLen = binary.BigEndian.Uint32(topicLenBuf)

	if topicLen < 1 {
		return false, false, "", ""
	}

	// Read topic
	topicBuf := make([]byte, topicLen)
	if _, err := conn.Read(topicBuf); err != nil {
		return false, false, "", ""
	}
	topic = string(topicBuf)

	// Read body length
	var bodyLen uint32
	bodyLenBuf := make([]byte, 4)
	if readLen, err := conn.Read(bodyLenBuf); err != nil || readLen != 4 {
		return false, false, "", ""
	}
	bodyLen = binary.BigEndian.Uint32(bodyLenBuf)
	if bodyLen <= 0 {
		return false, false, "", ""
	}

	// Read body
	bodyBuf := make([]byte, bodyLen)
	if _, err := conn.Read(bodyBuf); err != nil {
		return false, false, "", ""
	}
	message = string(bodyBuf)

	return true, isSubscribed, topic, message
}

func (m *TcpMessageSubscribe) createConnection() error {
	conn, err := net.Dial("tcp", fmt.Sprintf("%s:%d", m.serverAddr, m.serverPort))
	if err != nil {
		return fmt.Errorf("connection refused. Ensure the server is running on %s:%d: %v",
			m.serverAddr, m.serverPort, err)
	}

	m.mu.Lock()
	m.clientConn = conn
	m.connected = true
	m.mu.Unlock()

	return nil
}

func (m *TcpMessageSubscribe) startHeartbeat() {
	go m.sendPingText()
}

func (m *TcpMessageSubscribe) sendPingText() {
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
		case <-m.closeCh:
			return
		case <-ticker.C:
			m.mu.Lock()
			if !m.connected || m.clientConn == nil {
				m.mu.Unlock()
				return
			}
			conn := m.clientConn
			deadErr := conn.SetWriteDeadline(time.Now().Add(time.Second * 3))
			m.mu.Unlock()

			if deadErr != nil {
				m.Close()
				return
			}

			var buffer bytes.Buffer
			buffer.Write(topicLengthBytes)
			buffer.Write(topicBytes)
			buffer.Write(bodyLengthBytes)
			buffer.Write(bodyBytes)

			pingBytes := buffer.Bytes()

			if _, err := conn.Write(pingBytes); err != nil {
				m.Close()
				return
			}
		}
	}
}
