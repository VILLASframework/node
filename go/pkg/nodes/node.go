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
