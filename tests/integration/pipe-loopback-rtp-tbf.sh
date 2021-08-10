#!/bin/bash
#
# Integration loopback test for villas pipe.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @author Marvin Klimke <marvin.klimke@rwth-aachen.de>
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

FORMAT="villas.binary"
VECTORIZE="1"

RATE=500
NUM_SAMPLES=10000000
NUM_VALUES=5

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
			"rtcp": {
				"enabled": true,
				"mode": "aimd",
				"throttle_mode": "decimate"
			},
			"aimd": {
				"a": 10,
				"b": 0.75,
				"start_rate": ${RATE}
			},
			"in": {
				"address": "0.0.0.0:12002",
				"signals": {
					"count": ${NUM_VALUES},
					"type": "float"
				}
			},
			"out": {
				"address": "127.0.0.1:12000",
				"fwmark": 123
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
			"rtcp": {
				"enabled": true,
				"mode": "aimd",
				"throttle_mode": "decimate"
			},
			"in": {
				"address": "0.0.0.0:12000",
				"signals": {
					"count": ${NUM_VALUES},
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

tc qdisc del dev lo root
tc qdisc add dev lo root handle 4000 prio bands 4 priomap 1 2 2 2 1 2 0 0 1 1 1 1 1 1 1 1
tc qdisc add dev lo parent 4000:3 tbf rate 40kbps burst 32kbit latency 200ms #peakrate 40kbps mtu 1000 minburst 1520
tc filter add dev lo protocol ip handle 123 fw flowid 4000:3

villas pipe -l ${NUM_SAMPLES} dest.json rtp_node > output.dat &

sleep 1

villas signal mixed -v ${NUM_VALUES} -r ${RATE} -l ${NUM_SAMPLES} > input.dat

villas pipe src.json rtp_node > output.dat < input.dat

villas compare ${CMPFLAGS} input.dat output.dat

kill %%
wait %%
