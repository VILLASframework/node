#!/bin/bash
#
# Test protobuf serialization with Python client
#
# Author: Steffen Vogel <post@steffenvogel.de>
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

LAYER="unix"
FORMAT="protobuf"

NUM_SAMPLES=${NUM_SAMPLES:-20}
NUM_VALUES=${NUM_VALUES:-5}

case ${LAYER} in
	unix)
		LOCAL="/var/run/villas-node.server.sock"
		REMOTE="/var/run/villas-node.client.sock"
		;;

	udp)
		LOCAL="*:12000"
		REMOTE="127.0.0.1:12001"
		;;
esac

cat > config.json << EOF
{
	"nodes": {
		"py-client": {
			"type": "socket",
			"layer": "${LAYER}",
			"format": "${FORMAT}",

			"in": {
				"address": "${LOCAL}"
			},
			"out": {
				"address": "${REMOTE}"
			}
		}
	}
}
EOF

export PYTHONPATH=${BUILDDIR}/python:${SRCDIR}/python

# Start Python client in background
python3 ${SRCDIR}/clients/python/client.py unix &

# Wait for client to be ready
if [ "${LAYER}" = "unix" ]; then
	while [ ! -S "${REMOTE}" ]; do
		sleep 1
	done
fi

sleep 1

villas signal -l ${NUM_SAMPLES} -v ${NUM_VALUES} -n random > input.dat

villas pipe -l ${NUM_SAMPLES} config.json py-client > output.dat < input.dat

kill %%
wait %%

villas compare ${CMPFLAGS} input.dat output.dat
