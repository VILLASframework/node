#!/bin/bash
#
# Integration loopback test for villas pipe.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

NUM_SAMPLES=${NUM_SAMPLES:-10}

cat > config1.json << EOF
{
	"http": {
		"port": 8081
	},
	"nodes": {
		"node1": {
			"type": "websocket",

			"destinations": [
				"ws://127.0.0.1:8080/node2.protobuf"
			],

			"wait_connected": true
		}
	}
}
EOF

cat > config2.json << EOF
{
	"http": {
		"port": 8080
	},
	"nodes": {
		"node2": {
			"type": "websocket"
		}
	}
}
EOF

VILLAS_LOG_PREFIX="[signal] " \
villas signal -l ${NUM_SAMPLES} -n random > input.dat

VILLAS_LOG_PREFIX="[pipe2] " \
villas pipe -d debug -l ${NUM_SAMPLES} -r config2.json node2 > output.dat &

sleep 1

VILLAS_LOG_PREFIX="[pipe1] " \
villas pipe -d debug -L ${NUM_SAMPLES} -s config1.json node1 < input.dat

wait %%

villas compare input.dat output.dat
