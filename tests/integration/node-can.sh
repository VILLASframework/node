#!/bin/bash
#
# Integration can test using villas node.
#
# @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
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
# To set up vcan interface use the following commands 
# sudo modprobe vcan
# sudo ip link add dev vcan0 type vcan
# sudo ip link set vcan0 up

set -e

CAN_IF=$(ip link show type vcan | head -n1 | awk '{match($2, /(.*):/,a)}END{print a[1]}')

if [[ ! ${CAN_IF} ]]; then
    echo "Did not find any vcan interface"
    exit 99
fi

# Check if vcan0 interface is present
if [[ ! $(ip link show "${CAN_IF}" up) ]]; then
    echo "Interface ${CAN_IF} is not up"
    exit 99
fi

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

NUM_SAMPLES=${NUM_SAMPLES:-10}
NUM_VALUES=${NUM_VALUES:-3}

cat > config.json << EOF
{
	"nodes": {
		"can_node1": {
			"type": "can",
			"interface_name": "${CAN_IF}",
			"sample_rate": 500000,
			"in": {
				"signals": [
					{
						"name": "sigin1",
						"unit": "Volts",
						"type": "float",
						"enabled": true,
						"can_id": 66,
						"can_size": 4,
						"can_offset": 0
					},
					{
						"name": "sigin2",
						"unit": "Volts",
						"type": "float",
						"enabled": true,
						"can_id": 66,
						"can_size": 4,
						"can_offset": 4
					},
					{
						"name": "sigin3",
						"unit": "Volts",
						"type": "float",
						"enabled": true,
						"can_id": 67,
						"can_size": 8,
						"can_offset": 0
					}
				]
			},
			"out": {
				"signals": [
					{
						"type": "float",
						"can_id": 66,
						"can_size": 4,
						"can_offset": 0
					},
					{
						"type": "float",
						"can_id": 66,
						"can_size": 4,
						"can_offset": 4
					},
					{
						"type": "float",
						"can_id": 67,
						"can_size": 8,
						"can_offset": 0
					}
				]
			}
		}
	}
}
EOF

villas signal -v ${NUM_VALUES} -l ${NUM_SAMPLES} -n random > input.dat

villas node config.json &

# Wait for node to complete init
sleep 1

candump -n ${NUM_SAMPLES} -T 1000 -L ${CAN_IF} | awk '{print $3}' > can_out.dat &

# Send / Receive data to node
villas pipe -l ${NUM_SAMPLES} config.json can_node1 > output.dat < input.dat &

wait $candump

cat can_out.dat | xargs -I % cansend ${CAN_IF} %

# Wait for node to handle samples
sleep 1

kill %villas
wait %villas

villas compare input.dat output.dat
