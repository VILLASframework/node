#!/bin/bash
#
# Integration loopback test for villas pipe.
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
}
trap finish EXIT

NUM_SAMPLES=${NUM_SAMPLES:-10}
	
VECTORIZE="10"
FORMAT="protobuf"

cat > config.json << EOF
{
	"nodes": {
		"node1": {
			"type": "zeromq",

			"format": "${FORMAT}",
			"vectorize": ${VECTORIZE},
			"pattern": "pubsub",
			"out": {
				"publish": "tcp://127.0.0.1:12000"
			},
			"in": {
				"subscribe": "tcp://127.0.0.1:12000",
				"signals": {
					"type": "float",
					"count": 5
				}
			}
		}
	}
}
EOF

villas signal -l ${NUM_SAMPLES} -n -v 5 random > input.dat

villas pipe -l ${NUM_SAMPLES} config.json node1 > output.dat < input.dat

villas compare input.dat output.dat
