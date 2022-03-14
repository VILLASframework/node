/** Example of using libvillas node-types in Go code
 *
 * This example demonstrate how you can use VILLASnode's
 * node-types from a Go application.
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

package main

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
