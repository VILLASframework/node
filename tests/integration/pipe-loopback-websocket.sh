#!/bin/bash
#
# Integration loopback test for villas-pipe.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

# Test is broken
exit 99

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})
source ${SCRIPTPATH}/../../tools/villas-helper.sh

CONFIG_FILE=$(mktemp)
CONFIG_FILE2=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-100}

cat > ${CONFIG_FILE} << EOF
{
	"nodes" : {
		"node1" : {
			"type" : "websocket",

			"destinations" : [
				"ws://127.0.0.1:8080/node2.protobuf"
			]
		}
	}
}
EOF

cat > ${CONFIG_FILE2} << EOF
{
	"http" : {
		"port" : 8080
	},
	"nodes" : {
		"node2" : {
			"type" : "websocket"
		}
	}
}
EOF

# Generate test data
VILLAS_LOG_PREFIX=$(colorize "[Signal]") \
villas-signal -l ${NUM_SAMPLES} -n random > ${INPUT_FILE}

VILLAS_LOG_PREFIX=$(colorize "[Recv]  ") \
villas-pipe -r -l ${NUM_SAMPLES} ${CONFIG_FILE2} node2 | tee ${OUTPUT_FILE} &

sleep 1

VILLAS_LOG_PREFIX=$(colorize "[Send]  ") \
villas-pipe -s ${CONFIG_FILE} node1 < <(sleep 1; cat ${INPUT_FILE})

wait $!

# Compare data
villas-test-cmp ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

rm ${OUTPUT_FILE} ${INPUT_FILE} ${CONFIG_FILE} ${CONFIG_FILE2}

exit ${RC}
