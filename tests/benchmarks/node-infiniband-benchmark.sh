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
# the Free Software Foundation, either version 3 of the License, or any later version.
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
CONFIG_FILE_SOURCE=$(mktemp /tmp/ib-configuration-source-XXXX.conf)
INPUT_FILE=$(mktemp)
LOG_DIR=benchmarks_$(date +%Y%m%d_%H-%M-%S)
mkdir ${LOG_DIR}

NUM_VALUES=(2 4 8 16 32 64)
RATE_SAMPLES=(10 100 1000 10000 50000)
TIME_TO_RUN=30

# Declare modes
MODES=("RC" "UC" "UD")

# Initialize counter
COUNT=0

# Set target and source config file, which is the same for both runs
cat > ${CONFIG_FILE_TARGET} <<EOF
@include "${CONFIG_FILE//\/tmp\/}"

paths = (
    {
        in = "ib_node_target",
        out = "results_output"
    }
)
EOF

cat > ${CONFIG_FILE_SOURCE} <<EOF
@include "${CONFIG_FILE//\/tmp\/}"

paths = (
    {
        in = "siggen",
        out = ("ib_node_source", "results_input")
    }
)
EOF

# Run through modes
for MODE in "${MODES[@]}"
do
    for NUM_VALUE in "${NUM_VALUES[@]}"
    do
        for RATE_SAMPLE in "${RATE_SAMPLES[@]}"
        do
            NUM_SAMPLE=$((${RATE_SAMPLE} * ${TIME_TO_RUN}))

            echo "########################################################"
            echo "########################################################"
            echo "## START ${MODE}"
            echo "## NUM_VALUES: ${NUM_VALUE}"
            echo "## RATE_SAMPLES: ${RATE_SAMPLE}"
            echo "## NUM_SAMPLES: ${NUM_SAMPLE}"
            echo "########################################################"
            echo "########################################################"

            # Set config file with a MODE flag
cat > ${CONFIG_FILE} <<EOF
logging = {
    level = 0,
    facilities = "ib",
}

http = {
    enabled = false,
},

nodes = {
    siggen = {
        type = "signal",

        signal = "mixed",
        values = ${NUM_VALUE},
        frequency = 3,
        rate = ${RATE_SAMPLE},
        limit = ${NUM_SAMPLE},

        missed_signals = false,
    },

    results_input = {
        type = "file",

        format = "csv",
        uri = "${LOG_DIR}/${COUNT}_${MODE}-${NUM_VALUE}-${RATE_SAMPLE}-${NUM_SAMPLE}_input.csv",

        buffer_size = 2000000000,
    },

    results_output = {
        type = "file",

        format = "csv",
        uri = "${LOG_DIR}/${COUNT}_${MODE}-${NUM_VALUE}-${RATE_SAMPLE}-${NUM_SAMPLE}_output.csv",

        buffer_size = 2000000000,
    },

    ib_node_source = {
        type = "infiniband",

        rdma_port_space = "RDMA_PS_${MODE}",
        
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

            send_inline = true,
            max_inline_data = 60,

            use_fallback = true,
        }
    
    }

    ib_node_target = {
        type = "infiniband",

        rdma_port_space = "RDMA_PS_${MODE}",
        
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

            sleep 1

            # Start receiving node
            VILLAS_LOG_PREFIX=$(colorize "[Target Node]  ") \
            villas-node ${CONFIG_FILE_TARGET} &
            target_node_proc=$!
            
            # Wait for node to complete init
            sleep 2
            
            # Start sending pipe
            VILLAS_LOG_PREFIX=$(colorize "[Source Node]  ") \
            ip netns exec namespace0 villas-node ${CONFIG_FILE_SOURCE} &
            source_node_proc=$!
            
            sleep $((${TIME_TO_RUN} + 5))
            #sleep 5
            
            # Stop node
            kill $target_node_proc
            
            sleep 1

            echo "########################################################"
            echo "## STOP $MODE"-${NUM_VALUE}-${RATE_SAMPLE}-${NUM_SAMPLE}
            echo "########################################################"
            echo ""

            ((COUNT++))

            sleep 1
        done
    done
done

# Since this script will be executed as sudo we should chmod the
# log dir 777. Otherwise too many unnecessary 'sudo rm -rf' will be called.
chmod -R 777 ${LOG_DIR}

rm ${CONFIG_FILE} ${CONFIG_FILE_TARGET} ${CONFIG_FILE_SOURCE} ${INPUT_FILE} 

exit 0
