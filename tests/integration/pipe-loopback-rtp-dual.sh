#!/bin/bash
#
# Integration loopback test for villas pipe.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
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

FORMAT="villas.binary"
VECTORIZE="1"

RATE=100
NUM_SAMPLES=2000

cat > src.json << EOF
{
	"logging": {
		"level": "info"
	},
	"nodes": {
		"rtp_node": {
			"type": "rtp",
			"format": "${FORMAT}",
			"vectorize": ${VECTORIZE},
			"rate": ${RATE},
			"rtcp": true,
			"aimd": {
				"a": 10,
				"b": 0.5,
				"hook_type": "decimate"
			},
			"in": {
				"address": "0.0.0.0:12002",
				"signals": {
					"count": 5,
					"type": "float"
				}
			},
			"out": {
				"address": "127.0.0.1:12000"
			}
		}
	}
}
EOF

cat > dest.json << EOF
{
	"logging": {
		"level": "info"
	},
	"nodes": {
		"rtp_node": {
			"type": "rtp",
			"format": "${FORMAT}",
			"vectorize": ${VECTORIZE},
			"rate": ${RATE},
			"rtcp": true,
			"aimd": {
				"a": 10,
				"b": 0.5,

				"hook_type": "decimate"
			},
			"in": {
				"address": "0.0.0.0:12000",
				"signals": {
					"count": 5,
					"type": "float"
				}
			},
			"out": {
				"address": "127.0.0.1:12002"
			}
		}
	}
}
EOF

villas pipe -l ${NUM_SAMPLES} dest.json rtp_node > output.dat &

sleep 1

villas signal mixed -v 5 -r ${RATE} -l ${NUM_SAMPLES} > input.dat

villas pipe ${CONFIG_FILE_SRC} rtp_node > output.dat < input.dat

villas compare ${CMPFLAGS} input.dat output.dat

kill %%
wait %%
