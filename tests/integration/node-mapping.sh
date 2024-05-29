#!/usr/bin/env bash
#
# Integration loopback test using villas node.
#
# Test test checks if source mapping features for a path.
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

cat > expect.dat <<EOF
1637161831.766626638(1)	99	123	3.00000000000000000	12.00000000000000000
EOF

cat > config.json <<EOF
{
    "idle_stop": true,
    "nodes": {
        "sig_1": {
             "type": "signal.v2",

             "limit": 1,

             "initial_sequenceno": 99,

             "in": {
             	"signals": [
             		{ "name": "const1", "signal": "constant", "amplitude": 1 },
             		{ "name": "const2", "signal": "constant", "amplitude": 2 },
             		{ "name": "const3", "signal": "constant", "amplitude": 3 }
             	]
             }
        },
        "sig_2": {
             "type": "signal.v2",

             "limit": 1,

             "initial_sequenceno": 123,

             "in": {
             	"signals": [
             		{ "name": "const1", "signal": "constant", "amplitude": 11 },
             		{ "name": "const2", "signal": "constant", "amplitude": 12 },
             		{ "name": "const3", "signal": "constant", "amplitude": 13 }
             	]
             }
        },
        "file_1": {
             "type": "file",
             "uri": "output.dat",
             "in": {"hooks": ["print"]}
        }
    },
    "paths": [
        {
             "in": [
             	"sig_1.hdr.sequence",
             	"sig_2.hdr.sequence",
             	"sig_1.data[const3]",
             	"sig_2.data[const2]"
             ],
             "out": "file_1",
             "mode": "all"
        }
    ]
}
EOF

villas node -d debug  config.json

villas compare output.dat expect.dat
