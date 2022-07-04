/** Go types for signal configuration.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

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
