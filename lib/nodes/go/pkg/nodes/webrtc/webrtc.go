package nodes

import (
	"encoding/json"
	"net/url"

	node "git.rwth-aachen.de/acs/public/villas/node/pkg"
)

type WebRTCNode struct {
	node.Node

	Config WebRTCConfig
}

type WebRTCConfig struct {
	RendezvouzToken   string  `json:"token,omitempty"`
	RendezvouzAddress url.URL `json:"address,omitempty"`
}

func NewWebRTCNode() node.Node {
	return &WebRTCNode{}
}

func (n *WebRTCNode) Parse(c []byte) error {
	return json.Unmarshal(c, &n.Config)
}

func (n *WebRTCNode) Check() error {
	return nil
}

func (n *WebRTCNode) Prepare() error {
	return nil
}

func (n *WebRTCNode) Start() error {
	return nil
}

func (n *WebRTCNode) Stop() error {
	return nil
}

func (n *WebRTCNode) Read() ([]byte, error) {
	return nil, nil
}

func (n *WebRTCNode) Write(data []byte) error {
	return nil
}

func (n *WebRTCNode) PollFDs() []int {
	return nil
}

func (n *WebRTCNode) NetemFDs() []int {
	return nil
}

func (n *WebRTCNode) Details() string {
	return "this-is-my-webrtc-node"
}

func init() {
	node.RegisterNode("webrtc", NewWebRTCNode)
}
