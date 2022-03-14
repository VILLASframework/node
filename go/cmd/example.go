package main

/** Example of using libvillas node-types in Go code
 *
 * This example demonstrate how you can use VILLASnode's
 * node-types from a Go application.
 */

import (
	"log"
	"os"
	"time"

	"git.rwth-aachen.de/acs/public/villas/node/go/pkg"
	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/config"
	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/node"

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

	log.Printf("%s\n", n.NameFull())

	now := time.Now()

	smps_send := []node.Sample{
		{
			Sequence: 1234,
			Timestamps: pkg.Timestamps{
				Origin: pkg.Timestamp{now.Unix(), now.UnixNano()},
			},
			Data: []float64{1.1, 2.2, 3.3},
		},
		{
			Sequence: 1235,
			Timestamps: pkg.Timestamps{
				Origin: pkg.Timestamp{now.Unix(), now.UnixNano()},
			}, Data: []float64{4.4, 5.5, 6.6},
		},
	}

	log.Printf("Sent: %+#v\n", smps_send)

	cnt_written := n.Write(smps_send)
	smps_received := n.Read(cnt_written)

	log.Printf("Received: %+#v\n", smps_received)

	if len(smps_send) != len(smps_received) {
		log.Printf("Different length")
		os.Exit(-1)
	}

	if smps_send[0].Data[0] != smps_received[0].Data[0] {
		log.Printf("Different data")
		os.Exit(-1)
	}
}
