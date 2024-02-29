#!/usr/bin/env bash
#
# Integration loopback test using villas node.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
    popd
    rm -rf ${DIR}

    kill -SIGTERM 0 # kill all decendants
}
trap finish EXIT

NUM_SAMPLES=${NUM_SAMPLES:-10}

cat > config.json <<EOF
{
    "nodes": {
        "node1": {
             "type": "socket",
             "in": {
             	"address": "127.0.0.1:12000",
             	"signals": {
             		"type": "float",
             		"count": 1
             	}
             },
             "out": {
             	"address": "127.0.0.1:12001"
             }
        },
        "node2": {
             "type": "socket",
             "in": {
             	"address": "127.0.0.1:12001",
             	"signals": {
             		"type": "float",
             		"count": 1
             	}
             },
             "out": {
             	"address": "127.0.0.1:12000"
             }
        }
    },
    "paths": [
        {
             "in": "node1",
             "out": "node1"
        }
    ]
}
EOF

VILLAS_LOG_PREFIX="[signal] " \
villas signal -l ${NUM_SAMPLES} -n random > input.dat

VILLAS_LOG_PREFIX="[node] " \
villas node config.json &

# Wait for node to complete init
sleep 2

# Send / Receive data to node
VILLAS_LOG_PREFIX="[pipe] " \
villas pipe -l ${NUM_SAMPLES} config.json node2 > output.dat < input.dat

# Wait for node to handle samples
sleep 1

kill %%
wait %%

# Send / Receive data to node
VILLAS_LOG_PREFIX="[compare] " \
villas compare input.dat output.dat
