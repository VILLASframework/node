#!/bin/bash
#
# Integration loopback test for villas-pipe.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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
source ${SCRIPTPATH}/../../tools/integration-tests-helper.sh

CONFIG_FILE=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-100}

# Generate test data
villas-signal mixed -v 5 -l ${NUM_SAMPLES} -n > ${INPUT_FILE}

FORMAT="villas.binary"
VECTORIZE="1"

cat > ${CONFIG_FILE} << EOF
{
	"logging" : {
		"level" : "debug"
	},
	"nodes" : {
		"node1" : {
			"type" : "rtp",

			"format" : "${FORMAT}",
			"vectorize" : ${VECTORIZE},

			"in" : {
				"address" : "127.0.0.1:12000",

				"signals" : {
					"type" : "float",
					"count" : 5
				}
			},
			"out" : {
				"address" : "127.0.0.1:12000"
			}
		}
	}
}
EOF

villas-pipe -l ${NUM_SAMPLES} ${CONFIG_FILE} node1 > ${OUTPUT_FILE} < ${INPUT_FILE}

# Compare data
villas-test-cmp ${CMPFLAGS} ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

rm ${OUTPUT_FILE} ${INPUT_FILE} ${CONFIG_FILE}

exit $RC
