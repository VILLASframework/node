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
