#!/usr/bin/env bash
#
# Integration loopback test for villas pipe.
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

NUM_SAMPLES=${NUM_SAMPLES:-100}
FORMAT="villas.human"

cat > config.json << EOF
{
    "nodes": {
        "node1": {
             "type": "exec",
             "format": "${FORMAT}",

             "shell": true,

             "exec": "tee /tmp/test"
        }
    }
}
EOF

villas signal -l ${NUM_SAMPLES} -n random > input.dat

villas pipe -l ${NUM_SAMPLES} config.json node1 > output.dat < input.dat

villas compare input.dat output.dat
