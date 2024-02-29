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

RATE=500
NUM_SAMPLES=10000000
NUM_VALUES=5

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
			"rtcp": {
				"enabled": true,
				"mode": "aimd",
				"throttle_mode": "decimate"
			},
			"aimd": {
				"a": 10,
				"b": 0.75,
				"start_rate": ${RATE}
			},
			"in": {
				"address": "0.0.0.0:12002",
				"signals": {
					"count": ${NUM_VALUES},
					"type": "float"
				}
			},
			"out": {
				"address": "127.0.0.1:12000",
				"fwmark": 123
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
			"rtcp": {
				"enabled": true,
				"mode": "aimd",
				"throttle_mode": "decimate"
			},
			"in": {
				"address": "0.0.0.0:12000",
				"signals": {
					"count": ${NUM_VALUES},
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

tc qdisc del dev lo root
tc qdisc add dev lo root handle 4000 prio bands 4 priomap 1 2 2 2 1 2 0 0 1 1 1 1 1 1 1 1
tc qdisc add dev lo parent 4000:3 tbf rate 40kbps burst 32kbit latency 200ms #peakrate 40kbps mtu 1000 minburst 1520
tc filter add dev lo protocol ip handle 123 fw flowid 4000:3

villas pipe -l ${NUM_SAMPLES} dest.json rtp_node > output.dat &

sleep 1

villas signal mixed -v ${NUM_VALUES} -r ${RATE} -l ${NUM_SAMPLES} > input.dat

villas pipe src.json rtp_node > output.dat < input.dat

villas compare ${CMPFLAGS} input.dat output.dat

kill %%
wait %%
