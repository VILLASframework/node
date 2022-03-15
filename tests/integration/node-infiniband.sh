#!/bin/bash
#
# Integration Infiniband test using villas node.
#
# @author Dennis Potter <dennis@dennispotter.eu>
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

# Check if tools are present
if ! command -v lspci > /dev/null; then
	echo "lspci tool is missing"
	exit 99
fi

# Check if user is superuser. SU is used for namespace
if [[ "${EUID}" -ne 0 ]]; then
	echo "Please run as root"
	exit 99
fi

# Check if Infiniband card is present
if [[ ! $(lspci | grep Infiniband) ]]; then
	echo "Did not find any Infiniband cards in system"
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

# Set config file with a MODE flag
cat > config.json <<EOF
{
	"logging": {
		"level": 0,
		"facilities": "ib"
	},
	"http": {
		"enabled": false
	},
	"nodes": {
		"results": {
			"type": "file",
			"uri": "output.dat"
		},
		"ib_node_source": {
			"type": "infiniband",
			"rdma_port_space": "MODE",
			"in": {
				"address": "10.0.0.2:1337",
				"max_wrs": 8192,
				"cq_size": 8192,
				"poll_mode": "BUSY",
				"buffer_subtraction": 128
			},
			"out": {
				"address": "10.0.0.1:1337",
				"resolution_timeout": 1000,
				"max_wrs": 8192,
				"cq_size": 256,
				"send_inline": true,
				"max_inline_data": 60,
				"use_fallback": true
			}
		},
		"ib_node_target": {
			"type": "infiniband",
			"rdma_port_space": "MODE",
			"in": {
				"address": "10.0.0.1:1337",
				"max_wrs": 8192,
				"cq_size": 8192,
				"poll_mode": "BUSY",
				"buffer_subtraction": 128
			}
		}
	}
}
EOF

# Set target config file, which is the same for both runs
cat > target.json <<EOF
{
	"@include": "config.json",

	"paths": [
		{
			"in": "ib_node_target",
			"out": "results"
		}
    ]
}
EOF

villas signal -l ${NUM_SAMPLES} -n random > input.dat

# Declare modes
MODES=("RC" "UC" "UD")

# Run through modes
for MODE in "${MODES[@]}"; do

	echo "#############################"
	echo "#############################"
	echo "##       START ${MODE}     ##"
	echo "#############################"
	echo "#############################"

	sed -i -e 's/MODE/'${MODE}'/g' config.json 

	# Start receiving node
	villas node target.json &
	PID_PROC=$!
	
	# Wait for node to complete init
	sleep 1
	
	# Preprare fifo
	FIFO=$(mktemp -p ${DIR} -t)
	if [[ ! -p ${FIFO} ]]; then
		mkfifo ${FIFO}
	fi
	
	# Start sending pipe
	ip netns exec namespace0 \
    villas pipe -l ${NUM_SAMPLES} config.json ib_node_source >output.dat <${FIFO} & 
	PID_PIPE=$!
	
	# Keep pipe alive
	sleep 5 >${FIFO} &
	
	# Wait for pipe to connect to node
	sleep 3
	
	# Write data to pipe
	cat input.dat >${FIFO} &
	
	# Wait for node to handle samples
	sleep 2
	
	# Stop node
	kill ${PID_PIPE} 
	kill ${PID_PROC}
	
	villas compare input.dat output.dat

	sed -i -e 's/'${MODE}'/MODE/g' config.json 
done
