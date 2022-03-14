/** Exponential backoffs for reconnect timing.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

package webrtc

import "time"

var (
	DefaultExponentialBackoff = ExponentialBackoff{
		Factor:   1.5,
		Maximum:  1 * time.Minute,
		Initial:  1 * time.Second,
		Duration: 1 * time.Second,
	}
)

type ExponentialBackoff struct {
	Factor  float32
	Maximum time.Duration
	Initial time.Duration

	Duration time.Duration
}

func (e *ExponentialBackoff) Next() time.Duration {
	e.Duration = time.Duration(1.5 * float32(e.Duration)).Round(time.Second)
	if e.Duration > e.Maximum {
		e.Duration = e.Maximum
	}

	return e.Duration
}

func (e *ExponentialBackoff) Reset() {
	e.Duration = e.Initial
}
