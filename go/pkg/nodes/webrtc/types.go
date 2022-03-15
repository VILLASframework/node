/** Types for WebRTC node-type.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
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
