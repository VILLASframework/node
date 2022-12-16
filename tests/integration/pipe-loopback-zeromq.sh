#!/bin/bash
#
# Integration loopback test for villas pipe.
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
##################################################################################

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

NUM_SAMPLES=${NUM_SAMPLES:-10}
	
VECTORIZE="10"
FORMAT="protobuf"

cat > config.json << EOF
{
	"nodes": {
		"node1": {
			"type": "zeromq",

			"format": "${FORMAT}",
			"vectorize": ${VECTORIZE},
			"pattern": "pubsub",
			"out": {
				"publish": "tcp://127.0.0.1:12000"
			},
			"in": {
				"subscribe": "tcp://127.0.0.1:12000",
				"signals": {
					"type": "float",
					"count": 5
				}
			}
		}
	}
}
EOF

villas signal -l ${NUM_SAMPLES} -n -v 5 random > input.dat

villas pipe -l ${NUM_SAMPLES} config.json node1 > output.dat < input.dat

villas compare input.dat output.dat
