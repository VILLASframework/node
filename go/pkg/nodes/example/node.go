/** Little example node-type written in Go code.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
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

package nodes

import (
	"encoding/json"
	"fmt"
	"time"

	"git.rwth-aachen.de/acs/public/villas/node/go/pkg"
	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/errors"
	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/nodes"
)

type Node struct {
	nodes.BaseNode

	ticker time.Ticker

	lastSequence uint64

	Config Config
}

type Config struct {
	nodes.NodeConfig

	Value int `json:"value"`
}

func NewNode() nodes.Node {
	return &Node{
		BaseNode:     nodes.NewBaseNode(),
		ticker:       *time.NewTicker(1 * time.Second),
		lastSequence: 0,
	}
}

func (n *Node) Parse(c []byte) error {
	return json.Unmarshal(c, &n.Config)
}

func (n *Node) Check() error {
	return nil
}

func (n *Node) Prepare() error {
	return nil
}

func (n *Node) Start() error {
	n.Logger.Infof("hello from node")
	n.Logger.Warnf("hello from node")
	n.Logger.Errorf("hello from node")
	n.Logger.Tracef("hello from node")
	n.Logger.Criticalf("hello from node")

	return n.BaseNode.Start()
}

func (n *Node) Read() ([]byte, error) {
	select {
	case <-n.Stopped:
		return nil, errors.ErrEndOfFile

	case <-n.ticker.C:
		n.lastSequence++
		smp := pkg.GenerateRandomSample()
		smp.Sequence = n.lastSequence
		smps := []pkg.Sample{smp}

		return json.Marshal(smps)
	}
}

func (n *Node) Write(data []byte) error {
	n.Logger.Infof("Data: %s", string(data))

	return nil
}

func (n *Node) PollFDs() ([]int, error) {
	return []int{}, nil
}

func (n *Node) NetemFDs() ([]int, error) {
	return []int{}, nil
}

func (n *Node) Details() string {
	return fmt.Sprintf("value=%d", n.Config.Value)
}

func init() {
	// Do not forget to import the package in go/lib/main.go!
	nodes.RegisterNodeType("go.example", "A example node implemented in Go", NewNode, nodes.NodeSupportsRead|nodes.NodeSupportsWrite|nodes.NodeHidden)
}
