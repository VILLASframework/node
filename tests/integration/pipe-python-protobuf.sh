#!/bin/bash
#
# Test protobuf serialization with Python client
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})
SRCDIR=$(realpath ${SCRIPTPATH}/../..)
source ${SRCDIR}/tools/integration-tests-helper.sh

CONFIG_FILE=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

LAYER="unix"
FORMAT="protobuf"

NUM_SAMPLES=${NUM_SAMPLES:-20}
NUM_VALUES=${NUM_VALUES:-5}

# Generate test data
villas-signal -l ${NUM_SAMPLES} -v ${NUM_VALUES} -n random > ${INPUT_FILE}

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

cat > ${CONFIG_FILE} << EOF
{
	"nodes" : {
		"py-client" : {
			"type" : "socket",
			"layer": "${LAYER}",
			"format" : "${FORMAT}",

			"in" : {
				"address" : "${LOCAL}"
			},
			"out" : {
				"address" : "${REMOTE}"
			}
		}
	}
}
EOF

export PYTHONPATH=${BUILDDIR}/clients/python

# Start Python client in background
python ${SRCDIR}/clients/python/villas.py unix &
CPID=$!

# Wait for client to be ready
if [ "${LAYER}" = "unix" ]; then
	while [ ! -S "${REMOTE}" ]; do
		sleep 1
	done
fi
sleep 1

villas-pipe -l ${NUM_SAMPLES} ${CONFIG_FILE} py-client > ${OUTPUT_FILE} < ${INPUT_FILE}

kill ${CPID}

# Compare data
villas-test-cmp ${CMPFLAGS} ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

rm ${CONFIG_FILE} ${INPUT_FILE} ${OUTPUT_FILE}

exit $RC
