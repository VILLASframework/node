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

LOCAL_ADDR=137.226.133.195
REMOTE_ADDR=157.230.251.200
REMOTE_USER=root

REMOTE=ssh ${REMOTE_USER}@${REMOTE_ADDR}

export PATH=/projects/villas/node/build/src:${PATH}

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})
source ${SCRIPTPATH}/../../tools/integration-tests-helper.sh

CONFIG_FILE_SRC=$(mktemp)
CONFIG_FILE_DEST=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

FORMAT="villas.binary"
VECTORIZE="1"

RATE=100
NUM_SAMPLES=100

cat > ${CONFIG_FILE_SRC} << EOF
{
	"logging" : {
		"level" : "debug"
	},
	"nodes" : {
		"rtp_node" : {
			"type" : "rtp",
			"format" : "${FORMAT}",
			"vectorize" : ${VECTORIZE},
			"rate" : ${RATE},
			"rtcp" : {
				"enabled" : true,
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
				"address" : "${REMOTE_ADDR}:12000"
			}
		}
	}
}
EOF

# UDP ports: 33434 - 33534

${REMOTE} cat > ${CONFIG_FILE_DEST} << EOF
{
	"logging" : {
		"level" : "debug"
	},
	"nodes" : {
		"rtp_node" : {
			"type" : "rtp",
			"format" : "${FORMAT}",
			"vectorize" : ${VECTORIZE},
			"rate" : ${RATE},
			"rtcp": {
				"enabled" : true,
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
				"address" : "${LOCAL_ADDR}:12002"
			}
		}
	}
}
EOF

${REMOTE} villas-pipe -l ${NUM_SAMPLES} ${CONFIG_FILE_DEST} rtp_node > ${OUTPUT_FILE} &
PID=$!

sleep 1

villas-signal mixed -v 5 -r ${RATE} -l ${NUM_SAMPLES} | tee ${INPUT_FILE} | \
villas-pipe ${CONFIG_FILE_SRC} rtp_node > ${OUTPUT_FILE}

scp ${REMOTE_USER}@${REMOTE_ADDR}:${OUTPUT_FILE} ${OUTPUT_FILE}

# Compare data
villas-test-cmp ${CMPFLAGS} ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

rm ${INPUT_FILE} ${OUTPUT_FILE} ${CONFIG_FILE_SRC}
${REMOTE} rm ${OUTPUT_FILE} ${CONFIG_FILE_DEST}

kill ${PID}

exit $RC
