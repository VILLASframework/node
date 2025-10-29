#!/usr/bin/env bash
#
# Test for the webrtc node-type
#
# Author: Alexandra Bach <alexandra.bach@eonerc.rwth-aachen.de>
# SPDX-FileCopyrightText: 2014-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0
set -x
set -e

echo "The webrtc node restarts one peer, both peers, signaling server, and simulates connection loss."
# exit 99

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
    popd
    rm -rf ${DIR}
}
trap finish EXIT

SERVER="https://villas.k8s.eonerc.rwth-aachen.de/ws/signaling"
SESSION="villas-integration-test"
FORMAT="json"

cat > webrtc_siggen.json <<EOF
{
    "nodes": {
        "webrtc_siggen": {
            "type": "webrtc",
            "format": "${FORMAT}",
            "in": {
            "signals": [
                { "name": "signal", "type": "float", "unit": "unit" },
                { "name": "signal", "type": "float", "unit": "unit" },
                { "name": "signal", "type": "float", "unit": "unit" }
            ]
            },
        "out": {
            "signals": [
                { "name": "signal", "type": "float", "unit": "unit" },
                { "name": "signal", "type": "float", "unit": "unit" },
                { "name": "signal", "type": "float", "unit": "unit" }
            ]
            },
            "session": "${SESSION}",
            "server": "${SERVER}",
            "max_retransmits": 0,
            "wait_seconds": 10,
            "ordered": false,
            "ice": {
            }
        },
        "siggen": {
            "type": "signal",
            "signal": ["sine", "pulse", "square"],
            "values": 3,
            "rate": 1,
            "limit": 10
        }
    },
    "paths": [
        {
        "in": "siggen",
        "out": "webrtc_siggen",
        "hooks": [
            { "type": "print" }
        ]
        },
        {
        "in": "webrtc_siggen",
        "hooks": [
            { "type": "print" }
        ]
        }
    ],
    "logging": {
        "level": "info"
    }
}
EOF

cat > webrtc_loopback.json <<EOF
{
    "nodes": {
        "webrtc_loopback": {
            "type": "webrtc",
            "format": "${FORMAT}",
            "session": "${SESSION}",
            "server": "${SERVER}",
            "max_retransmits": 0,
            "ordered": false,
            "ice": {
            }
        }
    },
    "paths": [
        {
        "in": "webrtc_loopback",
        "out": "webrtc_loopback",
        "hooks": [
            { "type": "print" }
        ]
        }
    ]
}
EOF

# Start webrtc_loopback node in the background
villas node webrtc_loopback.json &
LOOPBACK_PID=$!
sleep 1

# Start webrtc_siggen node in the background
villas node webrtc_siggen.json &
SIGGEN_PID=$!
sleep 5

# Restart webrtc_siggen
kill $SIGGEN_PID
villas node webrtc_siggen.json &
SIGGEN_PID=$!
sleep 5

# Restart webrtc_loopback
kill $LOOPBACK_PID
villas node webrtc_loopback.json &
LOOPBACK_PID=$!
sleep 5

# Ensure cleanup on script exit
kill $SIGGEN_PID $LOOPBACK_PID

