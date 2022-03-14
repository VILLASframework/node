package webrtc

import (
	"fmt"
	"net/url"
	"time"

	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/nodes"
	"github.com/gorilla/websocket"
)

type SignalingClient struct {
	*websocket.Conn

	logger nodes.Logger

	URL *url.URL

	done chan struct{}

	isClosing bool
	backoff   ExponentialBackoff

	messageCallbacks    []func(msg *SignalingMessage)
	connectCallbacks    []func()
	disconnectCallbacks []func()
}

func NewSignalingClient(u *url.URL, logger nodes.Logger) (*SignalingClient, error) {
	c := &SignalingClient{
		messageCallbacks:    []func(msg *SignalingMessage){},
		connectCallbacks:    []func(){},
		disconnectCallbacks: []func(){},
		isClosing:           false,
		backoff:             DefaultExponentialBackoff,
		URL:                 u,
		logger:              logger,
	}

	return c, nil
}

func (c *SignalingClient) OnConnect(cb func()) {
	c.connectCallbacks = append(c.connectCallbacks, cb)
}

func (c *SignalingClient) OnDisconnect(cb func()) {
	c.disconnectCallbacks = append(c.connectCallbacks, cb)
}

func (c *SignalingClient) OnMessage(cb func(msg *SignalingMessage)) {
	c.messageCallbacks = append(c.messageCallbacks, cb)
}

func (c *SignalingClient) SendSignalingMessage(msg *SignalingMessage) error {
	c.logger.Infof("Sending signaling message: %s", msg)
	return c.Conn.WriteJSON(msg)
}

func (c *SignalingClient) Close() error {
	// Return immediatly if there is no open connection
	if c.Conn == nil {
		return nil
	}

	c.isClosing = true

	// Cleanly close the connection by sending a close message and then
	// waiting (with timeout) for the server to close the connection.
	err := c.Conn.WriteControl(websocket.CloseMessage, websocket.FormatCloseMessage(websocket.CloseNormalClosure, ""), time.Now().Add(5*time.Second))
	if err != nil {
		return fmt.Errorf("failed to send close message: %s", err)
	}

	select {
	case <-c.done:
		c.logger.Infof("Connection closed")
	case <-time.After(3 * time.Second):
		c.logger.Warn("Timed-out waiting for connection close")
	}

	return nil
}

func (c *SignalingClient) Connect() error {
	var err error

	dialer := websocket.Dialer{
		HandshakeTimeout: 1 * time.Second,
	}

	c.Conn, _, err = dialer.Dial(c.URL.String(), nil)
	if err != nil {
		return fmt.Errorf("failed to dial %s: %w", c.URL, err)
	}

	for _, cb := range c.connectCallbacks {
		cb()
	}

	go c.read()

	// Reset reconnect timer
	c.backoff.Reset()

	c.done = make(chan struct{})

	return nil
}

func (c *SignalingClient) ConnectWithBackoff() error {
	t := time.NewTimer(c.backoff.Duration)
	for range t.C {
		if err := c.Connect(); err != nil {
			t.Reset(c.backoff.Next())

			c.logger.Errorf("Failed to connect: %s. Reconnecting in %s", err, c.backoff.Duration)
		} else {
			break
		}
	}

	return nil
}

func (c *SignalingClient) read() {
	for {
		msg := &SignalingMessage{}
		if err := c.Conn.ReadJSON(msg); err != nil {
			if websocket.IsCloseError(err, websocket.CloseGoingAway, websocket.CloseNormalClosure) {

			} else {
				c.logger.Errorf("Failed to read: %s", err)
			}
			break
		}

		c.logger.Infof("Received signaling message: %s", msg)

		for _, cb := range c.messageCallbacks {
			cb(msg)
		}
	}

	c.closed()
}

func (c *SignalingClient) closed() {
	if err := c.Conn.Close(); err != nil {
		c.logger.Errorf("Failed to close connection: %s", err)
	}

	c.Conn = nil

	for _, cb := range c.disconnectCallbacks {
		cb()
	}

	close(c.done)

	if c.isClosing {
		c.logger.Infof("Connection closed")
	} else {
		c.logger.Warnf("Connection lost. Reconnecting in %s", c.backoff.Duration)
		go c.ConnectWithBackoff()
	}
}
