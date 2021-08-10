#!/bin/bash
#
# Test protobuf serialization with Python client
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASnode
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
