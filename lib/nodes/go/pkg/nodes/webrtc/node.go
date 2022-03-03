package webrtc

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/url"
	"strings"

	node "git.rwth-aachen.de/acs/public/villas/node/pkg"
	"github.com/pion/webrtc/v3"
)

var (
	DefaultConfig = Config{
		Server: &url.URL{
			Scheme: "wss",
			Host:   "https://ws-signal.villas.k8s.eonerc.rwth-aachen.de",
		},
		WebRTC: webrtc.Configuration{
			ICEServers: []webrtc.ICEServer{
				{
					URLs: []string{"stun:stun.l.google.com:19302"},
				},
			},
		},
	}
)

type Node struct {
	node.Node

	*PeerConnection

	Config Config

	receive chan []byte
	send    chan []byte

	stop chan struct{}
}

type Config struct {
	Server  *url.URL
	Session string

	WebRTC webrtc.Configuration
}

func NewNode() node.Node {
	return &Node{
		stop:    make(chan struct{}),
		send:    make(chan []byte),
		receive: make(chan []byte),

		Config: DefaultConfig,
	}
}

func (n *Node) Parse(c []byte) error {
	var err error
	var cfg struct {
		Session *string `json:"session"`
		Server  *string `json:"server,omitempty"`
		Ice     *struct {
			Servers []struct {
				URLs     []string `json:"urls,omitempty"`
				Username *string  `json:"username,omitempty"`
				Password *string  `json:"password,omitempty"`
			} `json:"servers,omitempty"`
		} `json:"ice,omitempty"`
	}

	if err := json.Unmarshal(c, &cfg); err != nil {
		return fmt.Errorf("failed to unmarshal config: %w", err)
	}

	if cfg.Session == nil || *cfg.Session == "" {
		return errors.New("missing or invalid session name")
	} else {
		n.Config.Session = *cfg.Session
	}

	if cfg.Server != nil {
		n.Config.Server, err = url.Parse(*cfg.Server)
		if err != nil {
			return fmt.Errorf("failed to parse server address: %w", err)
		}
	}

	if cfg.Ice != nil {
		for _, server := range cfg.Ice.Servers {
			iceServer := webrtc.ICEServer{
				URLs: server.URLs,
			}

			if server.Username != nil && server.Password != nil {
				iceServer.Username = *server.Username
				iceServer.CredentialType = webrtc.ICECredentialTypePassword
				iceServer.Credential = *server.Password
			}

			n.Config.WebRTC.ICEServers = append(n.Config.WebRTC.ICEServers, iceServer)
		}
	}

	return nil
}

func (n *Node) Check() error {
	return nil
}

func (n *Node) Prepare() error {
	var err error

	n.PeerConnection, err = NewPeerConnection(n.config)
	if err != nil {
		return fmt.Errorf("failed to create peer connection: %w", err)
	}

	return nil
}

func (n *Node) Start() error {
	return nil
}

func (n *Node) Stop() error {
	close(n.stop)

	return nil
}

func (n *Node) Read() ([]byte, error) {
	select {
	case <-n.stop:
	case b := <-n.receive:
		return b, nil
	}

	return nil, nil
}

func (n *Node) Write(data []byte) error {
	n.send <- data

	return nil
}

func (n *Node) PollFDs() []int {
	return []int{}
}

func (n *Node) NetemFDs() []int {
	return []int{}
}

func (n *Node) Details() string {
	details := map[string]string{
		"server":  n.Config.Server.String(),
		"session": n.Config.Session,
	}

	kv := []string{}
	for k, v := range details {
		kv = append(kv, fmt.Sprintf("%s=%s", k, v))
	}

	return strings.Join(kv, ", ")
}

func (n *Node) Close() error {
	return nil
}

func init() {
	node.RegisterNode("webrtc", NewNode)
}
