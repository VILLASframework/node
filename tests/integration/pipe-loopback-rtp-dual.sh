#!/bin/bash
#
# Integration loopback test for villas-pipe.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @author Marvin Klimke <marvin.klimke@rwth-aachen.de>
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

if [ -n "${CI}" ]; then
	echo "RTP tests are not ready yet"
	exit 99
fi

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})
source ${SCRIPTPATH}/../../tools/integration-tests-helper.sh

CONFIG_FILE_SRC=$(mktemp)
CONFIG_FILE_DEST=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

FORMAT="villas.binary"
VECTORIZE="1"

RATE=10000
NUM_SAMPLES=100

cat > ${CONFIG_FILE_SRC} << EOF
{
	"nodes" : {
		"rtp_node" : {
			"type" : "rtp",
			"format" : "${FORMAT}",
			"vectorize" : ${VECTORIZE},
			"rate" : ${RATE},
			"rtcp" : {
				"enabled" : false,
				"mode" : "aimd",
				"throttle_mode" : "decimate"
			},
			"aimd" : {
				"a" : 10,
				"b" : 0.5
			},
			"in" : {
				"address" : "0.0.0.0:12002",
				"signals" : {
					"count" : 5,
					"type" : "float"
				}
			},
			"out" : {
				"address" : "127.0.0.1:12000"
			}
		}
	}
}
EOF

cat > ${CONFIG_FILE_DEST} << EOF
{
	"nodes" : {
		"rtp_node" : {
			"type" : "rtp",
			"format" : "${FORMAT}",
			"vectorize" : ${VECTORIZE},
			"rate" : ${RATE},
			"rtcp": {
				"enabled" : false,
				"mode" : "aimd",
				"throttle_mode" : "decimate"
			},
			"aimd" : {
				"a" : 10,
				"b" : 0.5
			},
			"in" : {
				"address" : "0.0.0.0:12000",
				"signals" : {
					"count" : 5,
					"type" : "float"
				}
			},
			"out" : {
				"address" : "127.0.0.1:12002"
			}
		}
	}
}
EOF

villas-signal mixed -v 5 -l ${NUM_SAMPLES} > ${INPUT_FILE}

villas-pipe -l ${NUM_SAMPLES} ${CONFIG_FILE_DEST} rtp_node > ${OUTPUT_FILE} &

villas-pipe ${CONFIG_FILE_SRC} rtp_node < ${INPUT_FILE}

# Compare data
villas-test-cmp ${CMPFLAGS} ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

rm ${OUTPUT_FILE} ${INPUT_FILE} ${CONFIG_FILE}

exit $RC
