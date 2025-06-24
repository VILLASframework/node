#!/usr/bin/env bash
#
# Integration loopback test for villas pipe.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# Author: Marvin Klimke <marvin.klimke@rwth-aachen.de>
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
}
trap finish EXIT

FORMAT="villas.binary"
VECTORIZE="1"

RATE=100
NUM_SAMPLES=2000

cat > src.json << EOF
{
    "logging": {
        "level": "info"
    },
    "nodes": {
        "rtp_node": {
            "type": "rtp",
            "format": "${FORMAT}",
            "vectorize": ${VECTORIZE},
            "rate": ${RATE},
            "rtcp": true,
            "aimd": {
                "a": 10,
                "b": 0.5,
                "hook_type": "decimate"
            },
            "in": {
                "address": "0.0.0.0:12002",
                "signals": {
                    "count": 5,
                    "type": "float"
                }
            },
            "out": {
                "address": "127.0.0.1:12000"
            }
        }
    }
}
EOF

cat > dest.json << EOF
{
    "logging": {
        "level": "info"
    },
    "nodes": {
        "rtp_node": {
             "type": "rtp",
             "format": "${FORMAT}",
             "vectorize": ${VECTORIZE},
             "rate": ${RATE},
             "rtcp": true,
             "aimd": {
                 "a": 10,
                 "b": 0.5,

                 "hook_type": "decimate"
             },
             "in": {
                 "address": "0.0.0.0:12000",
                 "signals": {
                     "count": 5,
                     "type": "float"
                 }
             },
             "out": {
                 "address": "127.0.0.1:12002"
             }
        }
    }
}
EOF

villas pipe -l ${NUM_SAMPLES} dest.json rtp_node > output.dat &

sleep 1

villas signal mixed -v 5 -r ${RATE} -l ${NUM_SAMPLES} > input.dat

villas pipe ${CONFIG_FILE_SRC} rtp_node > output.dat < input.dat

villas compare ${CMPFLAGS} input.dat output.dat

kill %%
wait %%
