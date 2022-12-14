/** Sample datastructure.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
