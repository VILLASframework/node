#!/usr/bin/env bash
#
# Integration test for `c37.118` node-type.
#
# Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
# SPDX-FileCopyrightText: 2014-2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set -e

DIR=$(mktemp -d)
pushd ${DIR}

NUM_SAMPLES=${NUM_SAMPLES:-10}

cat > config.json <<EOF
{
    "nodes": {
        "file": {
            "type": "file",
            "uri": "input.dat"
        },
        "server": {
            "type": "c37.118",
            "hooks": ["print"],
            "out": {
                "address": "localhost",
                "idcode": 1,
                "testing": true,
                "data_rate": 10,
                "pmus": [{
                    "name": "VILLASnode",
                    "idcode": 1,
                    "nominal_frequency": 50,
                    "frequency": "signal0",
                    "rocof": "signal1",
                    "analog": [{ "signal": "signal2" }]
                }]
            }
        }
    },
    "paths": [{ "in": "file", "out": "server" }]
}
EOF

VILLAS_LOG_PREFIX="[signal] " villas signal -v 3 -l ${NUM_SAMPLES} -n sine -r 100 > input.dat
VILLAS_LOG_PREFIX="[server] " villas node config.json & sleep 0.2
VILLAS_LOG_PREFIX="[client] " villas pipe -x -r -l ${NUM_SAMPLES} config.json server > output.dat

echo "client has finished..."
kill %%
echo "server killed..."
wait %%
echo "server stopped..."

VILLAS_LOG_PREFIX="[compare] " villas compare -T input.dat output.dat
