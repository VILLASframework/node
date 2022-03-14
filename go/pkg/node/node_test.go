/** Unit tests for using libvillas in Go code.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

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
