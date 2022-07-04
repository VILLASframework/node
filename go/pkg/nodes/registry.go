/** Node-type registry.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

package nodes

var (
	goNodeTypes = map[string]NodeType{}
)

type NodeType struct {
	Constructor NodeConstructor
	Flags       int
	Description string
}

func RegisterNodeType(name string, desc string, ctor NodeConstructor, flags int) {
	goNodeTypes[name] = NodeType{
		Constructor: ctor,
		Flags:       flags,
		Description: desc,
	}
}

func NodeTypes() map[string]NodeType {
	return goNodeTypes
}
