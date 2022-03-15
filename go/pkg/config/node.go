/** Go types for node configuration.
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

package config

type Node struct {
	Name string `json:"name"`
	Type string `json:"type"`
}

type NodeDir struct {
}

type NodeLoopbackIn struct {
	NodeDir

	Signals []Signal      `json:"signals"`
	Hooks   []interface{} `json:"hooks"`
}

type LoopbackNode struct {
	Node

	In NodeLoopbackIn `json:"in"`
}
