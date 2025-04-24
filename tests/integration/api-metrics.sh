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
        "signal": {
             "type": "signal",
             "count": 10,
             "rate": 10,
             "signal": "sine"
        }
    },
    "paths": [
        {
             "in": "signal",
             "hooks": [
                "print"
             ]
        }
    ]
}
EOF

# Start VILLASnode instance with local config
villas node config.json &

# Wait for node to complete init
sleep 1

# Fetch config via API
curl -s http://localhost:8080/api/v2/metrics > metrics

# Shutdown VILLASnode
kill $!

cat metrics
