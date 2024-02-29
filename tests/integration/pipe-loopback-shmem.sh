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

NUM_SAMPLES=${NUM_SAMPLES:-10}
SIGNAL_COUNT=${SIGNAL_COUNT:-10}

for MODE in polling pthread; do
for VECTORIZE in 1 5; do

cat > config.json << EOF
{
    "nodes": {
        "node1": {
             "type": "shmem",
             "out": {
             	"name": "/villas-test"
             },
             "in": {
             	"name": "/villas-test"
             },
             "queuelen": 1024,
             "mode": "${MODE}",
             "vectorize": ${VECTORIZE}
        }
    }
}
EOF

villas signal -l ${NUM_SAMPLES} -v ${SIGNAL_COUNT} -n random > input.dat

villas pipe -l ${NUM_SAMPLES} config.json node1 > output.dat < input.dat

villas compare input.dat output.dat

done; done;
