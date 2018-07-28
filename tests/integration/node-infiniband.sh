#!/bin/bash
#
# Integration Infiniband test using villas-node.
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
if [[ "$EUID" -ne 0 ]]; then
    echo "Please run as root"
    exit 99
fi

# Check if Infiniband card is present
if [[ ! $(lspci | grep Infiniband) ]]; then
    echo "Did not find any Infiniband cards in system"
    exit 99
fi


SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})
source ${SCRIPTPATH}/../../tools/integration-tests-helper.sh

CONFIG_FILE=$(mktemp /tmp/ib-configuration-XXXX.conf)
CONFIG_FILE_TARGET=$(mktemp /tmp/ib-configuration-target-XXXX.conf)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-10}
RC = 0

# Generate test data for TCP and UDP test
VILLAS_LOG_PREFIX=$(colorize "[Signal]") \
villas-signal random -l ${NUM_SAMPLES} -n > ${INPUT_FILE}

# Set config file with a MODE flag
cat > ${CONFIG_FILE} <<EOF
nodes = {
    results = {
        type = "file",
        uri = "${OUTPUT_FILE}",
    },

    ib_node_source = {
        type = "infiniband",

        rdma_port_space = "RDMA_PS_MODE",
        
        in = {
            address = "10.0.0.2:1337",

            max_wrs = 8192,
            cq_size = 8192,

            poll_mode = "BUSY",
            buffer_subtraction = 128,
        },
        out = {
            address = "10.0.0.1:1337",
            resolution_timeout = 1000,
            
            max_wrs = 8192,
            cq_size = 256,

            send_inline = 1,
            max_inline_data = 60,
        }
    
    }

    ib_node_target = {
        type = "infiniband",

        rdma_port_space = "RDMA_PS_MODE",
        
        in = {
            address = "10.0.0.1:1337",

            max_wrs = 8192,
            cq_size = 8192,

            poll_mode = "BUSY",
            buffer_subtraction = 128,
        }
    }
}
EOF

# Set target config file, which is the same for both runs
cat > ${CONFIG_FILE_TARGET} <<EOF
@include "${CONFIG_FILE//\/tmp\/}"

paths = (
    {
        in = "ib_node_target",
        out = "results"
    }
)
EOF

# Declare modes
MODES=("TCP" "UDP")

# Run through modes
for MODE in "${MODES[@]}"
do

    echo "#############################"
    echo "#############################"
    echo "##       START ${MODE}         ##"
    echo "#############################"
    echo "#############################"

	sed -i -e 's/RDMA_PS_MODE/RDMA_PS_'${MODE}'/g' ${CONFIG_FILE}    

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
    node_pipe=$!
    
    # Keep pipe alive
    sleep 5 >${DATAFIFO} &
    
    # Wait for pipe to connect to node
    sleep 3
    
    # Write data to pipe
    cat ${INPUT_FILE} >${DATAFIFO} &
    
    # Wait for node to handle samples
    sleep 2
    
    # Stop node
    kill $node_pipe 
    kill $node_proc
    
    # Compare data
    villas-test-cmp ${INPUT_FILE} ${OUTPUT_FILE}
    RC=$?

    # Exit, if an error occurs
    if [[ $RC != 0 ]]; then
        rm ${CONFIG_FILE} ${CONFIG_FILE_TARGET} ${INPUT_FILE} ${OUTPUT_FILE} ${DATAFIFO}
    
        exit $RC
    fi

    echo "#############################"
    echo "#############################"
    echo "##       STOP  $MODE         ##"
    echo "#############################"
    echo "#############################"
    echo ""

	sed -i -e 's/RDMA_PS_'${MODE}'/RDMA_PS_MODE/g' ${CONFIG_FILE}    
done

rm ${CONFIG_FILE} ${CONFIG_FILE_TARGET} ${INPUT_FILE} ${OUTPUT_FILE} ${DATAFIFO}

exit $RC
