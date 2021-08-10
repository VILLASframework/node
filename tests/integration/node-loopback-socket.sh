#!/bin/bash
#
# Integration loopback test using villas node.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
	
	kill -SIGTERM 0 # kill all decendants
}
trap finish EXIT

NUM_SAMPLES=${NUM_SAMPLES:-10}

cat > config.json <<EOF
{
	"nodes": {
		"node1": {
			"type": "socket",
			"in": {
				"address": "127.0.0.1:12000",
				"signals": {
					"type": "float",
					"count": 1
				}
			},
			"out": {
				"address": "127.0.0.1:12001"
			}
		},
		"node2": {
			"type": "socket",
			"in": {
				"address": "127.0.0.1:12001",
				"signals": {
					"type": "float",
					"count": 1
				}
			},
			"out": {
				"address": "127.0.0.1:12000"
			}
		}
	},
	"paths": [
		{
			"in": "node1",
			"out": "node1"
		}
	]
}
EOF

VILLAS_LOG_PREFIX="[signal] " \
villas signal -l ${NUM_SAMPLES} -n random > input.dat

VILLAS_LOG_PREFIX="[node] " \
villas node config.json &

# Wait for node to complete init
sleep 2

# Send / Receive data to node
VILLAS_LOG_PREFIX="[pipe] " \
villas pipe -l ${NUM_SAMPLES} config.json node2 > output.dat < input.dat

# Wait for node to handle samples
sleep 1

kill %%
wait %%

# Send / Receive data to node
VILLAS_LOG_PREFIX="[compare] " \
villas compare input.dat output.dat
