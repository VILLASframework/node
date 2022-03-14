/** Sample datastructure.
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

package pkg

import (
	"encoding/json"
	"math/rand"
	"time"
)

type Timestamp [2]int64

func TimestampFromTime(t time.Time) Timestamp {
	return Timestamp{t.Unix(), int64(t.Nanosecond())}
}

type Timestamps struct {
	Origin   Timestamp `json:"origin"`
	Received Timestamp `json:"received"`
}

type Sample struct {
	Flags      int        `json:"-"`
	Timestamps Timestamps `json:"ts"`
	Sequence   uint64     `json:"sequence"`
	Data       []float64  `json:"data"`
}

func GenerateRandomSample() Sample {
	now := time.Now()

	return Sample{
		Timestamps: Timestamps{
			Origin: TimestampFromTime(now),
		},
		Sequence: 1,
		Data: []float64{
			1 * rand.Float64(),
			10 * rand.Float64(),
			100 * rand.Float64(),
		},
	}
}

func (s Sample) Bytes() []byte {
	b, _ := json.Marshal(s)
	return b
}

func (s Sample) Equal(t Sample) bool {
	if s.Flags != t.Flags {
		return false
	}

	if s.Sequence != t.Sequence {
		return false
	}

	if s.Timestamps != t.Timestamps {
		return false
	}

	if len(s.Data) != len(t.Data) {
		return false
	}

	for i, va := range s.Data {
		if vb := t.Data[i]; va != vb {
			return false
		}
	}

	return true
}
