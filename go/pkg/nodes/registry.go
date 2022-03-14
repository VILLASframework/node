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
