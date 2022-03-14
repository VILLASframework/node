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
