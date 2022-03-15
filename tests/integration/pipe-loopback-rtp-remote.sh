#!/bin/bash
#
# Integration loopback test for villas pipe.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @author Marvin Klimke <marvin.klimke@rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

set -e

echo "Test is broken"
exit 99

LOCAL_ADDR=137.226.133.195
REMOTE_ADDR=157.230.251.200
REMOTE_USER=root
REMOTE="ssh ${REMOTE_USER}@${REMOTE_ADDR}"

PATH=/projects/villas/node/build/src:${PATH}
${REMOTE} PATH=/projects/villas/node/build/src:${PATH}

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

FORMAT="villas.binary"
VECTORIZE="1"

RATE=100
NUM_SAMPLES=100

cat > src.json << EOF
{
	"logging": {
		"level": "debug"
	},
	"nodes": {
		"rtp_node": {
			"type": "rtp",
			"format": "${FORMAT}",
			"vectorize": ${VECTORIZE},
			"rate": ${RATE},
			"rtcp": {
				"enabled": true,
				"mode": "aimd",
				"throttle_mode": "decimate"
			},
			"aimd": {
				"a": 10,
				"b": 0.5
			},
			"in": {
				"address": "0.0.0.0:33466",
				"signals": {
					"count": 5,
					"type": "float"
				}
			},
			"out": {
				"address": "${REMOTE_ADDR}:33464"
			}
		}
	}
}
EOF

# UDP ports: 33434 - 33534

cat > dest.json << EOF
{
	"logging": {
		"level": "debug"
	},
	"nodes": {
		"rtp_node": {
			"type": "rtp",
			"format": "${FORMAT}",
			"vectorize": ${VECTORIZE},
			"rate": ${RATE},
			"rtcp": {
				"enabled": true,
				"mode": "aimd",
				"throttle_mode": "decimate"
			},
			"aimd": {
				"a": 10,
				"b": 0.5
			},
			"in": {
				"address": "0.0.0.0:33464",
				"signals": {
					"count": 5,
					"type": "float"
				}
			},
			"out": {
				"address": "${LOCAL_ADDR}:33466"
			}
		}
	}
}
EOF

scp dest.json ${REMOTE_USER}@${REMOTE_ADDR}:${CONFIG_FILE_DEST}

${REMOTE} villas pipe -l ${NUM_SAMPLES} ${CONFIG_FILE_DEST} rtp_node > output.dat &

sleep 1

villas signal mixed -v 5 -r ${RATE} -l ${NUM_SAMPLES} > input.dat

villas pipe src.json rtp_node < input.dat

scp ${REMOTE_USER}@${REMOTE_ADDR}:output.dat output.dat

villas compare ${CMPFLAGS} input.dat output.dat

${REMOTE} rm -f output.dat ${CONFIG_FILE_DEST}

kill %%
wait %%
