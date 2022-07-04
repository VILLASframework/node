#!/bin/bash
#
# Integration loopback test for villas pipe.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
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

NUM_SAMPLES=${NUM_SAMPLES:-100}
FORMAT="protobuf"	
VECTORIZE="10"
HOST="localhost"

if [ -n "${CI}" ]; then
	HOST="mosquitto"
else
	HOST="localhost"
fi

cat > config.json << EOF
{
	"nodes": {
		"node1": {
			"type": "mqtt",
			"format": "${FORMAT}",
			"vectorize": ${VECTORIZE},

			"username": "guest",
			"password": "guest",
			"host": "${HOST}",
			"port": 1883,
		
			"out": {
				"publish": "test-topic"
			},
			"in": {
				"subscribe": "test-topic"
			}
		}
	}
}
EOF

villas signal -l ${NUM_SAMPLES} -n random > input.dat

villas pipe -l ${NUM_SAMPLES} config.json node1 > output.dat < <(sleep 2; cat input.dat)

villas compare input.dat output.dat
