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
	HOST="rabbitmq"
else
	HOST="[::1]"
fi

cat > config.json << EOF
{
	"nodes": {
		"node1": {
			"type": "amqp",
			"format": "${FORMAT}",
			"vectorize": ${VECTORIZE},

			"uri": "amqp://guest:guest@${HOST}:5672/%2f",

			"exchange": "mytestexchange",
			"routing_key": "abc"
		}
	}
}
EOF

villas signal -l ${NUM_SAMPLES} -n random > input.dat

villas pipe -l ${NUM_SAMPLES} config.json node1 > output.dat < input.dat

villas compare ${CMPFLAGS} input.dat output.dat
