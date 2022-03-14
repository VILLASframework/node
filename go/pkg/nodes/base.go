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
