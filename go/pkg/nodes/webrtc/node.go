package webrtc

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/url"
	"strings"

	verrors "git.rwth-aachen.de/acs/public/villas/node/go/pkg/errors"
	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/nodes"
	"github.com/pion/webrtc/v3"
)

var (
	DefaultConfig = Config{
		Server: &url.URL{
			Scheme: "wss",
			Host:   "wss://ws-signal.villas.k8s.eonerc.rwth-aachen.de",
		},
		WebRTC: webrtc.Configuration{
			ICEServers: []webrtc.ICEServer{
				{
					// URLs: []string{"stun:stun.l.google.com:19302"},
					URLs: []string{
						"stun:edgy.0l.de:3478",
						"turn:edgy.0l.de:3478?transport=udp",
						"turn:edgy.0l.de:3478?transport=tcp",
					},
					CredentialType: webrtc.ICECredentialTypePassword,
					Username:       "villas",
					Credential:     "villas",
				},
			},
		},
	}
)

type Node struct {
	nodes.BaseNode
	*PeerConnection

	Config Config
}

type Config struct {
	Server  *url.URL
	Session string

	WebRTC webrtc.Configuration
}

func NewNode() nodes.Node {
	return &Node{
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

func (n *Node) Prepare() error {
	var err error

	n.PeerConnection, err = NewPeerConnection(&n.Config, n.Logger)
	if err != nil {
		return fmt.Errorf("failed to create peer connection: %w", err)
	}

	return nil
}

func (n *Node) Start() error {
	n.DataChannelLock.Lock()
	defer n.DataChannelLock.Unlock()

	n.Logger.Info("Waiting until datachannel is connected...")
	for n.DataChannel == nil {
		n.DataChannelConnected.Wait()
	}

	return n.BaseNode.Start()
}

func (n *Node) Read() ([]byte, error) {
	select {
	case <-n.Stopped:
		return nil, verrors.ErrEndOfFile
	case b := <-n.PeerConnection.ReceivedMessages:
		return b, nil
	}
}

func (n *Node) Write(data []byte) error {
	n.DataChannelLock.Lock()
	defer n.DataChannelLock.Unlock()

	if n.DataChannel == nil {
		n.logger.Infof("No datachannel open. Skipping sample...")
		return nil
	}

	return n.DataChannel.Send(data)
}

func (n *Node) PollFDs() ([]int, error) {
	return []int{}, nil
}

func (n *Node) NetemFDs() ([]int, error) {
	return []int{}, nil
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
	nodes.RegisterNodeType("webrtc", "Web Real-time Communication", NewNode, nodes.NodeSupportsRead|nodes.NodeSupportsWrite)
}
