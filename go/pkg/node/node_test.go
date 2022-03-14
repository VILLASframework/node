package node_test

import (
	"testing"
	"time"

	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/config"
	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/node"

	"github.com/google/uuid"
)

func TestNode(t *testing.T) {
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
		t.Fatalf("Failed to create node: %s", err)
	}
	defer n.Close()

	if err := n.Check(); err != nil {
		t.Fatalf("Failed to check node: %s", err)
	}

	if err := n.Prepare(); err != nil {
		t.Fatalf("Failed to prepare node: %s", err)
	}

	if err := n.Start(); err != nil {
		t.Fatalf("Failed to start node: %s", err)
	}
	defer n.Stop()

	t.Logf("%s", n.NameFull())

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

	t.Logf("Sent: %+#v", smps_send)

	cnt_written := n.Write(smps_send)
	if cnt_written != len(smps_send) {
		t.Fatalf("Failed to send all samples")
	}

	smps_received := n.Read(cnt_written)
	if len(smps_received) != cnt_written {
		t.Fatalf("Failed to receive samples back")
	}

	t.Logf("Received: %+#v", smps_received)
}
