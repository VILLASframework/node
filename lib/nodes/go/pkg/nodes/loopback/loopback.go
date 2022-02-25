package nodes

import (
	"encoding/json"
	"fmt"

	node "git.rwth-aachen.de/acs/public/villas/node/pkg"
)

type LoopbackNode struct {
	node.Node

	channel chan []byte

	Config LoopbackConfig
}

type LoopbackConfig struct {
	node.NodeConfig

	Value int `json:"value"`
}

func NewLoopbackNode() node.Node {
	return &LoopbackNode{
		channel: make(chan []byte, 1024),
	}
}

func (n *LoopbackNode) Parse(c []byte) error {
	return json.Unmarshal(c, &n.Config)
}

func (n *LoopbackNode) Check() error {
	return nil
}

func (n *LoopbackNode) Prepare() error {
	return nil
}

func (n *LoopbackNode) Start() error {
	return nil
}

func (n *LoopbackNode) Stop() error {
	return nil
}

func (n *LoopbackNode) Read() ([]byte, error) {
	return <-n.channel, nil
}

func (n *LoopbackNode) Write(data []byte) error {
	n.channel <- data

	return nil
}

func (n *LoopbackNode) PollFDs() []int {
	return nil
}

func (n *LoopbackNode) NetemFDs() []int {
	return nil
}

func (n *LoopbackNode) Details() string {
	return fmt.Sprintf("value=%d", n.Config.Value)
}

func init() {
	node.RegisterNode("go.loopback", NewLoopbackNode)
}
