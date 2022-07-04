/** Go types for node configuration.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
