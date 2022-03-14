/** Common code for implementing node-types in Go code.
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

type BaseNode struct {
	Node

	Logger Logger

	Stopped chan struct{}
}

func NewBaseNode() BaseNode {
	return BaseNode{}
}

func (n *BaseNode) Start() error {
	n.Stopped = make(chan struct{})
	return nil
}

func (n *BaseNode) Stop() error {
	close(n.Stopped)

	return nil
}

func (n *BaseNode) Parse(c []byte) error {
	return nil
}

func (n *BaseNode) Check() error {
	return nil
}

func (n *BaseNode) Restart() error {
	return nil
}

func (n *BaseNode) Pause() error {
	return nil
}

func (n *BaseNode) Resume() error {
	return nil
}

func (n *BaseNode) Reverse() error {
	return nil
}

func (n *BaseNode) Close() error {
	return nil
}

func (n *BaseNode) SetLogger(l Logger) {
	n.Logger = l
}
