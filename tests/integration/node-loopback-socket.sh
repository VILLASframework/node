#!/bin/bash
#
# Integration loopback test using villas-node.
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

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})
source ${SCRIPTPATH}/../../tools/villas-helper.sh

CONFIG_FILE=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-10}

cat > ${CONFIG_FILE} <<EOF
{
	"nodes": {
		"node1": {
			"type": "socket",
			"out" : {
				"address": "127.0.0.1:12001"
			},
			"in" : {
				"address": "127.0.0.1:12000",
				"signals" : {
					"type" : "float",
					"count" : 1
				}
			}
		},
		"node2": {
			"type": "socket",
			
			"out" : {
				"address": "127.0.0.1:12000"
			},
			"in" : {
				"address" : "127.0.0.1:12001",
				"signals" : {
					"type" : "float",
					"count" : 1
				}
			}
		}
	},
	"paths": [
		{
			"in": "node1",
			"out": "node1",
			"hooks" : [
				{ "type" : "print" }
			]
		}
	]
}
EOF

# Generate test data
VILLAS_LOG_PREFIX=$(colorize "[Signal]") \
villas-signal -l ${NUM_SAMPLES} -n random > ${INPUT_FILE}

# Start node
VILLAS_LOG_PREFIX=$(colorize "[Node]  ") \
villas-node ${CONFIG_FILE} &

# Wait for node to complete init
sleep 1

# Send / Receive data to node
VILLAS_LOG_PREFIX=$(colorize "[Pipe]  ") \
villas-pipe -l ${NUM_SAMPLES} ${CONFIG_FILE} node2 > ${OUTPUT_FILE} < ${INPUT_FILE}

# Wait for node to handle samples
sleep 1

# Stop node
kill %1

# Compare data
villas-test-cmp ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

rm ${CONFIG_FILE} ${INPUT_FILE} ${OUTPUT_FILE}

exit ${RC}
