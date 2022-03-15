/** Node interface.
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

const (
	NodeSupportsPoll    = (1 << iota)
	NodeSupportsRead    = (1 << iota)
	NodeSupportsWrite   = (1 << iota)
	NodeRequiresWeb     = (1 << iota)
	NodeProvidesSignals = (1 << iota)
	NodeInternal        = (1 << iota)
	NodeHidden          = (1 << iota)
)

type NodeConstructor func() Node

type Node interface {
	Close() error

	Prepare() error

	Parse(cfg []byte) error

	Check() error

	Start() error
	Stop() error

	Pause() error
	Resume() error
	Restart() error

	Read() ([]byte, error)
	Write(data []byte) error

	Reverse() error

	PollFDs() ([]int, error)
	NetemFDs() ([]int, error)

	Details() string

	SetLogger(l Logger)
}

type NodeConfig struct {
	Type string `json:"type"`

	In struct {
	} `json:"in"`

	Out struct {
	} `json:"out"`
}
