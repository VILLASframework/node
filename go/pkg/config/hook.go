/** Go types for hook configuration.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

package config

type Hook struct {
	Type     string `json:"type"`
	Priority int    `json:"priority,omitempty"`
}

type PrintHook struct {
	Hook
}
