package config

const (
	SignalTypeFloat   = "float"
	SignalTypeInteger = "integer"
	SignalTypeBoolean = "boolean"
	SignalTypeComplex = "complex"
)

type Signal struct {
	Name string  `json:"name,omitempty"`
	Type string  `json:"type,omitempty"`
	Unit string  `json:"unit,omitempty"`
	Init float64 `json:"init,omitempty"`
}
