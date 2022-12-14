#!/bin/bash
#
# Integration loopback test for villas pipe.
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
##################################################################################

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

RATE=1000
NUM_SAMPLES=$((10*${RATE}))

cat > config.json << EOF
{
	"logging": {
		"level": "debug"
	},
	"nodes": {
		"node1": {
			"type": "rtp",

			"format": "${FORMAT}",
			"vectorize": ${VECTORIZE},

			"rtcp": {
				"enabled": true,
				"throttle_mode": "limit_rate"
			},

			"aimd": {
				"start_rate": 1,
				"a": 10,
				"b": 0.5
			},

			"in": {
				"address": "127.0.0.1:12000",

				"signals": {
					"type": "float",
					"count": 5
				}
			},
			"out": {
				"address": "127.0.0.1:12000"
			}
		}
	}
}
EOF

villas signal mixed -v 5 -r ${RATE} -l ${NUM_SAMPLES} > input.dat

villas pipe -l ${NUM_SAMPLES} config.json node1 > output.dat < intput.dat

villas compare ${CMPFLAGS} input.dat output.dat
