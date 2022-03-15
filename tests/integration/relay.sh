#!/bin/bash
#
# Integration test for villas relay
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
	kill ${PID_RELAY}
}
trap finish EXIT

NUM_SAMPLES=100

cat > config.json << EOF
{
	"nodes": {
		"relay1": {
			"type": "websocket",

			"wait_connected": true,
            "destinations": [
                "http://localhost:8123/node"
            ]
		},

        "relay2": {
			"type": "websocket",

			"wait_connected": true,
            "destinations": [
                "http://localhost:8123/node"
            ]
		}
	}
}
EOF

VILLAS_LOG_PREFIX="[signal] " \
villas signal -l ${NUM_SAMPLES} -n random > input.dat

# Test with loopback

VILLAS_LOG_PREFIX="[relay] " \
villas relay -l -p 8123 &
PID_RELAY=$!

VILLAS_LOG_PREFIX="[pipe1] " \
villas pipe config.json relay1 < input.dat > output.dat

VILLAS_LOG_PREFIX="[compare] " \
villas compare input.dat output.dat

# VILLAS_LOG_PREFIX="[pipe2-recv] " \
# villas pipe config.json -r relay2 > output.dat &

# sleep 0.1

# VILLAS_LOG_PREFIX="[pipe2-send] " \
# villas pipe config.json -s relay1 < input.dat

# VILLAS_LOG_PREFIX="[compare] " \
# villas compare input.dat output.dat
