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

if [ -n "${CI}" ]; then
  # We skip this test for now in CI
  echo "Test not yet supported"
  exit 99
fi	

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})
source ${SCRIPTPATH}/../../tools/villas-helper.sh

CONFIG_FILE=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

FORMAT="villas.binary"
VECTORIZE="1"

RATE=1000
NUM_SAMPLES=$((10*${RATE}))

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

			"rtcp" : {
				"enabled" : true,
				"throttle_mode" : "limit_rate"
			},

			"aimd" : {
				"start_rate" : 1,
				"a" : 10,
				"b" : 0.5
			},

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

villas-signal mixed -v 5 -r ${RATE} -l ${NUM_SAMPLES} | tee ${INPUT_FILE} | \
villas-pipe -l ${NUM_SAMPLES} ${CONFIG_FILE} node1 > ${OUTPUT_FILE}

# Compare data
villas-compare ${CMPFLAGS} ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

rm ${OUTPUT_FILE} ${INPUT_FILE} ${CONFIG_FILE}

exit ${RC}
