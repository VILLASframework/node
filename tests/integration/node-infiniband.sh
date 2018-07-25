#!/bin/bash
#
# Integration loopback test using villas-node.
#
# @author Dennis Potter <dennis@dennispotter.eu>
# @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

# Check if user is superuser. SU is used for namespace
if [ "$EUID" -ne 0 ]
    then echo "Please run as root"
    exit 99
fi

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})
source ${SCRIPTPATH}/../../tools/integration-tests-helper.sh

CONFIG_FILE=$(mktemp)
CONFIG_FILE_TARGET=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-100}

cat > ${CONFIG_FILE} <<EOF
nodes: {
    results = {
        type = "file",
        
        uri = "/dev/null",

        in = {
        },
        out = {
        },
    },

    ib_node_source = {
        type = "infiniband",

        rdma_port_space = "RDMA_PS_TCP",
        
        in = {
            address = "10.0.0.2:1337",

            max_wrs = 8192,
            cq_size = 8192,

            vectorize = 1,

            poll_mode = "BUSY",
            buffer_subtraction = 128,
        },
        out = {
            address = "10.0.0.1:1337",
            resolution_timeout = 1000,
            
            max_wrs = 8192,
            cq_size = 256,

            vectorize = 1,

            send_inline = 1,
            max_inline_data = 60,
        }
    
    }

    ib_node_target = {
        type = "infiniband",

        rdma_port_space = "RDMA_PS_TCP",
        
        in = {
            address = "10.0.0.1:1337",

            max_wrs = 8192,
            cq_size = 8192,

            vectorize = 1,

            poll_mode = "BUSY",
            buffer_subtraction = 128,

            hooks = (
                { type = "stats", verbose = true }
            )
        },
        out = {
            address = "10.0.0.2:1337",
            resolution_timeout = 1000,
            
            max_wrs = 8192,
            cq_size = 256,

            vectorize = 1,

            send_inline = 1,
            max_inline_data = 60,
        }
	}
}
EOF

cat > ${CONFIG_FILE_TARGET} <<EOF
$(<${CONFIG_FILE})
paths = (
    {
        in = "ib_node_target",
        out = "ib_node_target"

		hooks : (
			{ type = "print" }
        )
    }
)
EOF

## Generate test data
VILLAS_LOG_PREFIX=$(colorize "[Signal]") \
villas-signal random -l ${NUM_SAMPLES} -n > ${INPUT_FILE}

# Start receiving node
VILLAS_LOG_PREFIX=$(colorize "[Node]  ") \
villas-node ${CONFIG_FILE_TARGET} &
node_proc=$!

# Wait for node to complete init
sleep 1

# Preprare fifo
DATAFIFO=/tmp/datafifo
if [[ ! -p ${DATAFIFO} ]]; then
    mkfifo ${DATAFIFO}
fi

# Start sending pipe
VILLAS_LOG_PREFIX=$(colorize "[Pipe]  ") \
ip netns exec namespace0 villas-pipe -l ${NUM_SAMPLES} ${CONFIG_FILE} ib_node_source >${OUTPUT_FILE} <${DATAFIFO} & 

# Keep pipe alive
sleep 5 >${DATAFIFO} &

# Wait for pipe to connect to node
sleep 3

# Write data to pipe
cat ${INPUT_FILE} >${DATAFIFO} &

# Wait for node to handle samples
sleep 2

# Stop node
kill %1
kill -9 $node_proc

# Compare data
villas-test-cmp ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

rm ${CONFIG_FILE} ${CONFIG_FILE_TARGET} ${INPUT_FILE} ${OUTPUT_FILE} ${DATAFIFO}

exit $RC
