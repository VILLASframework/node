#!/usr/bin/env bash
#
# Integration test for villas relay
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

echo "Test is broken"
exit 99

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
    popd
    rm -rf ${DIR}
    kill ${PID_RELAY}
}
trap finish EXIT

NUM_SAMPLES=100

cat > config.json << EOF
{
    "nodes": {
        "relay1": {
             "type": "websocket",

             "wait_connected": true,
            "destinations": [
                "http://localhost:8123/node"
            ]
        },

        "relay2": {
             "type": "websocket",

             "wait_connected": true,
            "destinations": [
                "http://localhost:8123/node"
            ]
        }
    }
}
EOF

VILLAS_LOG_PREFIX="[signal] " \
villas signal -l ${NUM_SAMPLES} -n random > input.dat

# Test with loopback

VILLAS_LOG_PREFIX="[relay] " \
villas relay -l -p 8123 &
PID_RELAY=$!

VILLAS_LOG_PREFIX="[pipe1] " \
villas pipe config.json relay1 < input.dat > output.dat

VILLAS_LOG_PREFIX="[compare] " \
villas compare input.dat output.dat

# VILLAS_LOG_PREFIX="[pipe2-recv] " \
# villas pipe config.json -r relay2 > output.dat &

# sleep 0.1

# VILLAS_LOG_PREFIX="[pipe2-send] " \
# villas pipe config.json -s relay1 < input.dat

# VILLAS_LOG_PREFIX="[compare] " \
# villas compare input.dat output.dat
