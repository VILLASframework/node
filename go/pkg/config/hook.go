package config

type Hook struct {
	Type     string `json:"type"`
	Priority int    `json:"priority,omitempty"`
}

type PrintHook struct {
	Hook
}
