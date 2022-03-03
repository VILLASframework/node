package webrtc

import (
	"fmt"
	"time"

	"github.com/pion/webrtc/v3"
	"github.com/sirupsen/logrus"
)

type PeerConnection struct {
	*webrtc.PeerConnection
	*SignalingClient

	config *Config

	makingOffer bool
	ignoreOffer bool

	first  bool
	polite bool

	rollback bool
}

func NewPeerConnection(config *Config) (*PeerConnection, error) {
	sc, err := NewSignalingClient(config.Server)
	if err != nil {
		return nil, fmt.Errorf("failed to create signaling client: %w", err)
	}

	ppc := &PeerConnection{
		SignalingClient: sc,
		config:          config,
	}

	ppc.SignalingClient.OnMessage(ppc.OnSignalingMessageHandler)
	ppc.SignalingClient.OnConnect(ppc.OnSignalingConnectedHandler)

	if err := ppc.SignalingClient.ConnectWithBackoff(); err != nil {
		return nil, fmt.Errorf("failed to connect signaling client: %w", err)
	}

	return ppc, nil
}

func (pc *PeerConnection) createPeerConnection() (*webrtc.PeerConnection, error) {
	logrus.Info("Created new peer connection")

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

	if !pc.first {
		dc, err := ppc.CreateDataChannel("demo", nil)
		if err != nil {
			logrus.Panicf("Failed to create datachannel: %s", err)
		}

		close := make(chan struct{})

		dc.OnClose(func() { pc.OnDataChannelCloseHandler(dc, close) })
		dc.OnOpen(func() { pc.OnDataChannelOpenHandler(dc, close) })
		dc.OnMessage(func(msg webrtc.DataChannelMessage) { pc.OnDataChannelMessageHandler(dc, &msg, close) })
	}

	return ppc, nil
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

func (pc *PeerConnection) OnDataChannelOpenHandler(dc *webrtc.DataChannel, close chan struct{}) {
	logrus.Infof("DataChannel opened: %s", dc.Label())

	pc.SendMessages(dc, close)
}

func (pc *PeerConnection) OnDataChannelCloseHandler(dc *webrtc.DataChannel, cl chan struct{}) {
	logrus.Infof("DataChannel closed: %s", dc.Label())

	close(cl)

	// We close the connection here to avoid waiting for the disconnected event
	if err := pc.PeerConnection.Close(); err != nil {
		logrus.Errorf("Failed to close peer connection: %s", err)
	}
}

func (pc *PeerConnection) OnDataChannelMessageHandler(dc *webrtc.DataChannel, msg *webrtc.DataChannelMessage, close chan struct{}) {
	logrus.Infof("Received: %s", string(msg.Data))
}

func (pc *PeerConnection) SendMessages(dc *webrtc.DataChannel, close chan struct{}) {
	logrus.Info("Start sending messages")

	i := 0
	t := time.NewTicker(1 * time.Second)

loop:
	for {
		select {
		case <-t.C:
			msg := fmt.Sprintf("Hello %d", i)

			logrus.Infof("Send: %s", msg)

			if err := dc.SendText(msg); err != nil {
				logrus.Errorf("Failed to send message: %s", err)
				break loop
			}

		case <-close:
			break loop
		}

		i++
	}

	logrus.Info("Stopped sending messages")
}

func (pc *PeerConnection) OnDataChannelHandler(dc *webrtc.DataChannel) {
	logrus.Infof("New DataChannel opened: %s", dc.Label())

	close := make(chan struct{})

	dc.OnOpen(func() { pc.OnDataChannelOpenHandler(dc, close) })
	dc.OnClose(func() { pc.OnDataChannelCloseHandler(dc, close) })
	dc.OnMessage(func(msg webrtc.DataChannelMessage) { pc.OnDataChannelMessageHandler(dc, &msg, close) })
}

func (pc *PeerConnection) OnICECandidateHandler(c *webrtc.ICECandidate) {
	if c == nil {
		logrus.Info("Candidate gathering concluded")
		return
	}

	logrus.Infof("Found new candidate: %s", c)

	if err := pc.SendSignalingMessage(&SignalingMessage{
		Candidate: c,
	}); err != nil {
		logrus.Errorf("Failed to send candidate: %s", err)
	}
}

func (pc *PeerConnection) OnNegotiationNeededHandler() {
	logrus.Info("Negotation needed!")

	pc.makingOffer = true

	offer, err := pc.CreateOffer(nil)
	if err != nil {
		logrus.Panicf("Failed to create offer: %s", err)
	}

	if err := pc.SetLocalDescription(offer); err != nil {
		logrus.Panicf("Failed to set local description: %s", err)
	}

	if err := pc.SendSignalingMessage(&SignalingMessage{
		Description: &offer,
	}); err != nil {
		logrus.Panicf("Failed to send offer: %s", err)
	}

	pc.makingOffer = false
}

func (pc *PeerConnection) OnSignalingStateChangeHandler(ss webrtc.SignalingState) {
	logrus.Infof("Signaling State has changed: %s", ss.String())
}

func (pc *PeerConnection) OnConnectionStateChangeHandler(pcs webrtc.PeerConnectionState) {
	logrus.Infof("Connection State has changed: %s", pcs.String())

	switch pcs {
	case webrtc.PeerConnectionStateFailed:
		fallthrough
	case webrtc.PeerConnectionStateDisconnected:
		logrus.Info("Closing peer connection")

		if err := pc.PeerConnection.Close(); err != nil {
			logrus.Panicf("Failed to close peer connection: %s", err)
		}

	case webrtc.PeerConnectionStateClosed:
		if pc.rollback {
			return
		}

		logrus.Info("Closed peer connection")

		var err error
		pc.PeerConnection, err = pc.createPeerConnection()
		if err != nil {
			logrus.Panicf("Failed to set re-create peer connection: %s", err)
		}
	}
}

func (pc *PeerConnection) OnSignalingConnectedHandler() {
	var err error

	logrus.Info("Signaling connected")

	// Create initial peer connection
	if pc.PeerConnection == nil {
		if pc.PeerConnection, err = pc.createPeerConnection(); err != nil {
			logrus.Warnf("failed to create peer connection: %s", err)
		}
	}
}

func (pc *PeerConnection) OnSignalingMessageHandler(msg *SignalingMessage) {
	var err error

	if msg.Control != nil {
		if len(msg.Control.Connections) > 2 {
			logrus.Panicf("There are already two peers connected to this session.")
		}

		pc.first = true
		for _, c := range msg.Control.Connections {
			if c.ID < msg.Control.ConnectionID {
				pc.first = false
			}
		}

		pc.polite = pc.first

		logrus.Infof("New role: polite=%v, first=%v", pc.polite, pc.first)
	} else if msg.Description != nil {
		readyForOffer := !pc.makingOffer && pc.SignalingState() == webrtc.SignalingStateStable
		offerCollision := msg.Description.Type == webrtc.SDPTypeOffer && !readyForOffer

		pc.ignoreOffer = !pc.polite && offerCollision
		if pc.ignoreOffer {
			return
		}

		if msg.Description.Type == webrtc.SDPTypeOffer && pc.PeerConnection.SignalingState() != webrtc.SignalingStateStable {
			pc.PeerConnection, err = pc.rollbackPeerConnection()
			if err != nil {
				logrus.Panicf("Failed to rollback peer connection: %s", err)
			}
		}

		if err := pc.PeerConnection.SetRemoteDescription(*msg.Description); err != nil {
			logrus.Panicf("Failed to set remote description: %s", err)
		}

		if msg.Description.Type == webrtc.SDPTypeOffer {
			answer, err := pc.PeerConnection.CreateAnswer(nil)
			if err != nil {
				logrus.Panicf("Failed to create answer: %s", err)
			}

			if err := pc.SetLocalDescription(answer); err != nil {
				logrus.Panicf("Failed to rollback signaling state: %s", err)
			}

			pc.SendSignalingMessage(&SignalingMessage{
				Description: pc.LocalDescription(),
			})
		}
	} else if msg.Candidate != nil {
		if err := pc.AddICECandidate(msg.Candidate.ToJSON()); err != nil && !pc.ignoreOffer {
			logrus.Panicf("Failed to add new ICE candidate: %s", err)
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
