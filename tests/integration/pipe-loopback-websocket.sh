#!/bin/bash
#
# Integration loopback test for villas pipe.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
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

NUM_SAMPLES=${NUM_SAMPLES:-10}

cat > config1.json << EOF
{
	"http": {
		"port": 8081
	},
	"nodes": {
		"node1": {
			"type": "websocket",

			"destinations": [
				"ws://127.0.0.1:8080/node2.protobuf"
			],

			"wait_connected": true
		}
	}
}
EOF

cat > config2.json << EOF
{
	"http": {
		"port": 8080
	},
	"nodes": {
		"node2": {
			"type": "websocket"
		}
	}
}
EOF

VILLAS_LOG_PREFIX="[signal] " \
villas signal -l ${NUM_SAMPLES} -n random > input.dat

VILLAS_LOG_PREFIX="[pipe2] " \
villas pipe -d debug -l ${NUM_SAMPLES} -r config2.json node2 > output.dat &

sleep 1

VILLAS_LOG_PREFIX="[pipe1] " \
villas pipe -d debug -L ${NUM_SAMPLES} -s config1.json node1 < input.dat

wait %%

villas compare input.dat output.dat
