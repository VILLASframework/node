package main

import (
	"fmt"
	"log"
	"time"

	node "git.rwth-aachen.de/acs/public/villas/node/go/pkg"
	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/config"

	"github.com/google/uuid"
)

func main() {
	cfg := &config.LoopbackNode{
		Node: config.Node{
			Name: "lo1",
			Type: "loopback",
		},
		In: config.NodeLoopbackIn{
			Hooks: []interface{}{
				&config.PrintHook{
					Hook: config.Hook{
						Type: "print",
					},
				},
			},
			Signals: []config.Signal{
				{
					Name: "sig1",
				},
				{
					Name: "sig2",
				}, {
					Name: "sig3",
				},
			},
		},
	}

	n, err := node.NewNode(cfg, uuid.New())
	if err != nil {
		log.Fatalf("Failed to create node: %s", err)
	}
	defer n.Close()

	if err := n.Check(); err != nil {
		log.Fatalf("Failed to check node: %s", err)
	}

	if err := n.Prepare(); err != nil {
		log.Fatalf("Failed to prepare node: %s", err)
	}

	if err := n.Start(); err != nil {
		log.Fatalf("Failed to start node: %s", err)
	}
	defer n.Stop()

	fmt.Printf("%s\n", n.NameFull())

	smps_send := []node.Sample{
		{
			Sequence:        1234,
			TimestampOrigin: time.Now(),
			Data:            []float64{1.1, 2.2, 3.3},
		},
		{
			Sequence:        1235,
			TimestampOrigin: time.Now(),
			Data:            []float64{4.4, 5.5, 6.6},
		},
	}

	fmt.Printf("Sent: %+#v\n", smps_send)

	cnt_written := n.Write(smps_send)
	smps_received := n.Read(cnt_written)

	fmt.Printf("Received: %+#v\n", smps_received)
}
