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
