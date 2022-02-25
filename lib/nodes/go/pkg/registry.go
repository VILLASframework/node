package node

var (
	goNodeTypes = map[string]NodeCtor{}
)

func RegisterNode(name string, ctor NodeCtor) {
	goNodeTypes[name] = ctor
}

func NodeTypes() map[string]NodeCtor {
	return goNodeTypes
}
