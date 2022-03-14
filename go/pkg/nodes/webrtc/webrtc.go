/** WebRTC peer connection handling.
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

import (
	"fmt"
	"sync"

	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/nodes"
	"github.com/pion/webrtc/v3"
)

type PeerConnection struct {
	*webrtc.PeerConnection
	*SignalingClient

	config *Config

	logger nodes.Logger

	makingOffer bool
	ignoreOffer bool

	first  bool
	polite bool

	rollback bool

	DataChannel          *webrtc.DataChannel
	DataChannelLock      sync.Mutex
	DataChannelConnected *sync.Cond

	ReceivedMessages chan []byte
}

func NewPeerConnection(config *Config, logger nodes.Logger) (*PeerConnection, error) {
	u := *config.Server
	u.Path = "/" + config.Session
	sc, err := NewSignalingClient(&u, logger)
	if err != nil {
		return nil, fmt.Errorf("failed to create signaling client: %w", err)
	}

	ppc := &PeerConnection{
		SignalingClient: sc,
		config:          config,
		logger:          logger,

		DataChannel:      nil,
		ReceivedMessages: make(chan []byte, 1024),
	}

	ppc.DataChannelConnected = sync.NewCond(&ppc.DataChannelLock)

	ppc.SignalingClient.OnMessage(ppc.OnSignalingMessageHandler)
	ppc.SignalingClient.OnConnect(ppc.OnSignalingConnectedHandler)

	if err := ppc.SignalingClient.ConnectWithBackoff(); err != nil {
		return nil, fmt.Errorf("failed to connect signaling client: %w", err)
	}

	return ppc, nil
}

func (pc *PeerConnection) createPeerConnection() (*webrtc.PeerConnection, error) {
	pc.logger.Info("Created new peer connection")

	// Create a new RTCPeerConnection
	ppc, err := webrtc.NewPeerConnection(pc.config.WebRTC)
	if err != nil {
		return nil, fmt.Errorf("failed to create peer connection: %w", err)
	}

	// Set the handler for ICE connection state
	// This will notify you when the peer has connected/disconnected
	ppc.OnConnectionStateChange(pc.OnConnectionStateChangeHandler)
	ppc.OnSignalingStateChange(pc.OnSignalingStateChangeHandler)
	ppc.OnICECandidate(pc.OnICECandidateHandler)
	ppc.OnNegotiationNeeded(pc.OnNegotiationNeededHandler)
	ppc.OnDataChannel(pc.OnDataChannelHandler)

	return ppc, nil
}

func (pc *PeerConnection) createDataChannel() (*webrtc.DataChannel, error) {
	dc, err := pc.CreateDataChannel("villas", nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create datachannel: %w", err)
	}

	close := make(chan struct{})

	dc.OnClose(func() { pc.OnDataChannelCloseHandler(dc, close) })
	dc.OnOpen(func() { pc.OnDataChannelOpenHandler(dc, close) })
	dc.OnMessage(func(msg webrtc.DataChannelMessage) { pc.OnDataChannelMessageHandler(dc, &msg, close) })

	return dc, nil
}

func (pc *PeerConnection) rollbackPeerConnection() (*webrtc.PeerConnection, error) {
	pc.rollback = true
	defer func() { pc.rollback = false }()

	// Close previous peer connection in before creating a new one
	// We need to do this as pion/webrtc currently does not support rollbacks
	if err := pc.PeerConnection.Close(); err != nil {
		return nil, fmt.Errorf("failed to close peer connection: %w", err)
	}

	if ppc, err := pc.createPeerConnection(); err != nil {
		return nil, err
	} else {
		return ppc, nil
	}
}

func (pc *PeerConnection) OnConnectionCreated() error {
	if !pc.first {
		if _, err := pc.createDataChannel(); err != nil {
			return err
		}
	}

	return nil
}

func (pc *PeerConnection) OnDataChannelOpenHandler(dc *webrtc.DataChannel, close chan struct{}) {
	pc.logger.Infof("DataChannel opened: %s", dc.Label())

	pc.DataChannelLock.Lock()
	defer pc.DataChannelLock.Unlock()

	pc.DataChannel = dc

	pc.DataChannelConnected.Broadcast()
}

func (pc *PeerConnection) OnDataChannelCloseHandler(dc *webrtc.DataChannel, cl chan struct{}) {
	pc.logger.Infof("DataChannel closed: %s", dc.Label())

	pc.DataChannel = nil

	close(cl)

	// We close the connection here to avoid waiting for the disconnected event
	if err := pc.PeerConnection.Close(); err != nil {
		pc.logger.Errorf("Failed to close peer connection: %s", err)
	}
}

func (pc *PeerConnection) OnDataChannelMessageHandler(dc *webrtc.DataChannel, msg *webrtc.DataChannelMessage, close chan struct{}) {
	pc.logger.Debugf("Received: %s", string(msg.Data))

	pc.ReceivedMessages <- msg.Data
}

func (pc *PeerConnection) OnDataChannelHandler(dc *webrtc.DataChannel) {
	pc.logger.Infof("New DataChannel opened: %s", dc.Label())

	close := make(chan struct{})

	dc.OnOpen(func() { pc.OnDataChannelOpenHandler(dc, close) })
	dc.OnClose(func() { pc.OnDataChannelCloseHandler(dc, close) })
	dc.OnMessage(func(msg webrtc.DataChannelMessage) { pc.OnDataChannelMessageHandler(dc, &msg, close) })
}

func (pc *PeerConnection) OnICECandidateHandler(c *webrtc.ICECandidate) {
	if c == nil {
		pc.logger.Info("Candidate gathering concluded")
		return
	}

	pc.logger.Infof("Found new candidate: %s", c)

	ci := c.ToJSON()
	if err := pc.SendSignalingMessage(&SignalingMessage{
		Candidate: &ci,
	}); err != nil {
		pc.logger.Errorf("Failed to send candidate: %s", err)
	}
}

func (pc *PeerConnection) OnNegotiationNeededHandler() {
	pc.logger.Info("Negotation needed!")

	pc.makingOffer = true

	offer, err := pc.CreateOffer(nil)
	if err != nil {
		pc.logger.Panicf("Failed to create offer: %s", err)
	}

	if err := pc.SetLocalDescription(offer); err != nil {
		pc.logger.Panicf("Failed to set local description: %s", err)
	}

	if err := pc.SendSignalingMessage(&SignalingMessage{
		Description: &offer,
	}); err != nil {
		pc.logger.Panicf("Failed to send offer: %s", err)
	}

	pc.makingOffer = false
}

func (pc *PeerConnection) OnSignalingStateChangeHandler(ss webrtc.SignalingState) {
	pc.logger.Infof("Signaling State has changed: %s", ss.String())
}

func (pc *PeerConnection) OnConnectionStateChangeHandler(pcs webrtc.PeerConnectionState) {
	pc.logger.Infof("Connection State has changed: %s", pcs.String())

	switch pcs {
	case webrtc.PeerConnectionStateFailed:
		fallthrough
	case webrtc.PeerConnectionStateDisconnected:
		pc.logger.Info("Closing peer connection")

		if err := pc.PeerConnection.Close(); err != nil {
			pc.logger.Panicf("Failed to close peer connection: %s", err)
		}

	case webrtc.PeerConnectionStateClosed:
		if pc.rollback {
			return
		}

		pc.logger.Info("Closed peer connection")

		var err error
		pc.PeerConnection, err = pc.createPeerConnection()
		if err != nil {
			pc.logger.Panicf("Failed to set re-create peer connection: %s", err)
		}

		if err := pc.OnConnectionCreated(); err != nil {
			pc.logger.Panicf("Failed to create connection: %w", err)
		}
	}
}

func (pc *PeerConnection) OnSignalingConnectedHandler() {
	var err error

	pc.logger.Info("Signaling connected")

	// Create initial peer connection
	if pc.PeerConnection == nil {
		if pc.PeerConnection, err = pc.createPeerConnection(); err != nil {
			pc.logger.Panicf("Failed to create peer connection: %s", err)
		}

		if err := pc.OnConnectionCreated(); err != nil {
			pc.logger.Panicf("Failed to create connection: %s", err)
		}
	}
}

func (pc *PeerConnection) OnSignalingMessageHandler(msg *SignalingMessage) {
	var err error

	if msg.Control != nil {
		if len(msg.Control.Connections) > 2 {
			pc.logger.Panicf("There are already two peers connected to this session.")
		}

		// The peer with the smallest connection ID connected first
		pc.first = true
		for _, c := range msg.Control.Connections {
			if c.ID < msg.Control.ConnectionID {
				pc.first = false
			}
		}

		pc.polite = pc.first

		pc.logger.Infof("New role: polite=%v, first=%v", pc.polite, pc.first)
	} else if msg.Description != nil {
		readyForOffer := !pc.makingOffer && pc.SignalingState() == webrtc.SignalingStateStable
		offerCollision := msg.Description.Type == webrtc.SDPTypeOffer && !readyForOffer

		pc.ignoreOffer = !pc.polite && offerCollision
		if pc.ignoreOffer {
			return
		}

		if msg.Description.Type == webrtc.SDPTypeOffer && pc.PeerConnection.SignalingState() != webrtc.SignalingStateStable {
			if pc.PeerConnection, err = pc.rollbackPeerConnection(); err != nil {
				pc.logger.Panicf("Failed to rollback peer connection: %s", err)
			}

			if err := pc.OnConnectionCreated(); err != nil {
				pc.logger.Panicf("Failed to create connection: %s", err)
			}
		}

		if err := pc.PeerConnection.SetRemoteDescription(*msg.Description); err != nil {
			pc.logger.Panicf("Failed to set remote description: %s", err)
		}

		if msg.Description.Type == webrtc.SDPTypeOffer {
			answer, err := pc.PeerConnection.CreateAnswer(nil)
			if err != nil {
				pc.logger.Panicf("Failed to create answer: %s", err)
			}

			if err := pc.SetLocalDescription(answer); err != nil {
				pc.logger.Panicf("Failed to rollback signaling state: %s", err)
			}

			pc.SendSignalingMessage(&SignalingMessage{
				Description: pc.LocalDescription(),
			})
		}
	} else if msg.Candidate != nil {
		if err := pc.AddICECandidate(*msg.Candidate); err != nil && !pc.ignoreOffer {
			pc.logger.Panicf("Failed to add new ICE candidate: %s", err)
		}
	}
}

func (pc *PeerConnection) Close() error {
	if err := pc.SignalingClient.Close(); err != nil {
		return fmt.Errorf("failed to close signaling client: %w", err)
	}

	if err := pc.PeerConnection.Close(); err != nil {
		return fmt.Errorf("failed to close peer connection: %w", err)
	}

	return nil
}
