package node

type NodeCtor func() Node

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

	GetPollFDs() ([]int, error)
	GetNetemFDs() ([]int, error)

	// GetMemoryType()

	Details() string
}

type NodeConfig struct {
	Type string `json:"type"`

	In struct {
	} `json:"in"`

	Out struct {
	} `json:"out"`
}
