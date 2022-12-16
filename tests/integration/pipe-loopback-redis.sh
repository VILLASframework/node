#!/bin/bash
#
# Integration loopback test for villas pipe.
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
##################################################################################

echo "Test not ready"
exit 99

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT
NUM_SAMPLES=${NUM_SAMPLES:-100}

HOST="localhost"

if [ -n "${CI}" ]; then
	HOST="redis"
fi

cat > config.json << EOF
{
	"nodes": {
		"node1": {
			"type": "redis",
			"format": "protobuf",
			"vectorize": 10,
		
			"uri": "tcp://${HOST}:6379/0"
		}
	}
}
EOF

villas signal -l ${NUM_SAMPLES} -n random > input.dat

villas pipe -l ${NUM_SAMPLES} config.json node1 < input.dat > output.dat

villas compare input.dat output.dat
