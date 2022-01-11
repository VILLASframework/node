#!/bin/bash
#
# Integration test for villas relay
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

NUM_SAMPLES=100

cat > config.json << EOF
{
	"nodes": {
		"relay1": {
			"type": "websocket",

            "destinations": [
                "http://localhost:8123/node"
            ]
		}

        "relay2": {
			"type": "websocket",

            "destinations": [
                "http://localhost:8123/node"
            ]
		}
	}
}
EOF

villas signal -l ${NUM_SAMPLES} -n random > input.dat

# Test with loopback

villas relay -l -p 8123 &

villas pipe config.json relay1 < input.dat > output.dat

villas compare input.dat output.dat

kill %%
wait %%

rm output.dat

# Test without loopback
villas relay -p 8123 &

villas pipe config.json -s relay1 < input.dat &
villas pipe config.json -r relay2 > output.dat

kill %1 %2
wait %1 %2

villas compare input.dat output.dat
