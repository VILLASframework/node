/** Types for WebRTC node-type.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

package webrtc

import (
	"encoding/json"
	"time"

	"github.com/pion/webrtc/v3"
)

type Connection struct {
	ID int `json:"id"`

	Remote    string    `json:"remote"`
	UserAgent string    `json:"user_agent"`
	Created   time.Time `json:"created"`
}

type ControlMessage struct {
	ConnectionID int          `json:"connection_id"`
	Connections  []Connection `json:"connections"`
}

type Role struct {
	Polite bool `json:"polite"`
	First  bool `json:"first"`
}

type SignalingMessage struct {
	Description *webrtc.SessionDescription `json:"description,omitempty"`
	Candidate   *webrtc.ICECandidateInit   `json:"candidate,omitempty"`
	Control     *ControlMessage            `json:"control,omitempty"`
}

func (msg SignalingMessage) String() string {
	b, _ := json.Marshal(msg)
	return string(b)
}
