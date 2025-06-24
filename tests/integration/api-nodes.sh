#!/usr/bin/env bash
#
# Integration test for remote API
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
}
trap finish EXIT


cat > config.json <<EOF
{
    "http": {
        "port": 8080
    },
    "nodes": {
        "testnode1": {
            "type": "websocket",
            "dummy": "value1"
        },
        "testnode2": {
            "type": "socket",
            "dummy": "value2",

            "in": {
                "address": "*:12001",
                "signals": [
                    { "name": "sig1", "unit": "Volts",  "type": "float", "init": 123.0 },
                    { "name": "sig2", "unit": "Ampere", "type": "integer", "init": 123 }
                ]
            },
            "out": {
                "address": "127.0.0.1:12000"
            }
        }
    },
    "paths": [
        {
            "in": "testnode2",
            "out": "testnode1"
        }
    ]
}
EOF

# Start VILLASnode instance with local config
villas node config.json &

# Wait for node to complete init
sleep 1

# Fetch config via API
curl -s http://localhost:8080/api/v2/nodes > fetched.json

# Shutdown VILLASnode
kill $!

# Compare local config with the fetched one
jq -e '.[0].name == "testnode1" and .[0].type == "websocket" and (. | length == 2)' fetched.json > /dev/null
