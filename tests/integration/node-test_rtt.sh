#!/usr/bin/env bash
#
# Integration loopback test using villas node.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

echo "Test not yet supported"
exit 99

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
    popd
    rm -rf ${DIR}
}
trap finish EXIT

mkdir ./logs

cat > config.json <<EOF
{
    "logging": { "level": 2 },
    "stats": 1,
    "nodes": {
        "test": {
            "type": "test_rtt",
            "cooldown": 2,
            "output": "./logs",
            "cases": [
                {
                    "rates": 55.0,
                    "values": 5,
                    "limit": 100
                },
                {
                    "rates": [ 10, 10, 1000 ],
                    "values": [ 2, 10, 20, 50 ],
                    "duration": 5
                }
            ],
            "hooks": [
                {
                    "type": "stats",
                    "verbose": false
                }
            ]
        }
    },
    "paths": [
        {
            "in": "test",
            "out": "test"
        }
    ]
}
EOF

villas node config.json
