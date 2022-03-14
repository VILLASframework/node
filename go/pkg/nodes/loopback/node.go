/** Simple loopback node-type written in Go code.
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

package nodes

import (
	"encoding/json"
	"fmt"

	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/errors"
	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/nodes"
)

type Node struct {
	nodes.BaseNode

	channel chan []byte

	Config LoopbackConfig
}

type LoopbackConfig struct {
	nodes.NodeConfig

	Value int `json:"value"`
}

func NewNode() nodes.Node {
	return &Node{
		BaseNode: nodes.NewBaseNode(),
		channel:  make(chan []byte, 1024),
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
	return n.BaseNode.Start()
}

func (n *Node) Read() ([]byte, error) {
	select {
	case <-n.Stopped:
		return nil, errors.ErrEndOfFile

	case buf := <-n.channel:
		return buf, nil
	}
}

func (n *Node) Write(data []byte) error {
	n.channel <- data

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
	nodes.RegisterNodeType("go.loopback", "A loopback node implmented in Go", NewNode, nodes.NodeSupportsRead|nodes.NodeSupportsWrite|nodes.NodeHidden)
}
