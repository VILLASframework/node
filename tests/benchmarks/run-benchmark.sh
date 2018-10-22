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

######################################
# SETTINGS ###########################
######################################

# ${NUM_VALUES}, ${RATE_SAMPLES}, and ${IB_MODES} may be a list.

NUM_VALUES=(8)
RATE_SAMPLES=(10)
TIME_TO_RUN=5
IB_MODES=("RC")

######################################
######################################
######################################

# Check if user is superuser. SU is used for namespace
if [[ "$EUID" -ne 0 ]]; then
    echo "Please run as root"
    exit 99
fi

# Check whether cpuset cset command is availble
if [[ ! $(command -v cset) ]]; then
    echo "Cset is not availble for root. Please install it: https://github.com/lpechacek/cpuset"
    exit 99
fi

# Check if Infiniband card is present
if [[ ! $(lspci | grep Infiniband) ]]; then
    echo "Did not find any Infiniband cards in system"
    exit 99
fi

# Create list of configs and check whether they are valid
OIFS=$IFS; IFS=$'\n'; CONFIG_FILES=($(ls configs | sed -e "s/.conf//")); IFS=$OIFS;

NODETYPES=()

if [[ ! $1 ]]; then
    echo "Please define for which node-type to run the script"
    exit 1
elif [[ $1 == all ]]; then
    echo "Benchmarking the following nodes:"
    for NODETYPE in "${CONFIG_FILES[@]}"
    do
        echo ${NODETYPE}
        NODETYPES+=(${NODETYPE})
    done

else
    FOUND=0

    for NODETYPE in "${CONFIG_FILES[@]}"
    do
        if [[ $1 == ${NODETYPE} ]]; then
            NODETYPES=$1
            FOUND=1
            break
        fi
    done

    if [[ ${FOUND} == 0 ]]; then
        echo "Please define a valid node-type for which a config file is present in ./configs!"
        exit 1
    fi
fi

######################################
# SET PATHS ##########################
######################################
# Set paths
SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})
source ${SCRIPTPATH}/../../tools/integration-tests-helper.sh

# Declare location of config files
CONFIG=$(mktemp /tmp/nodetype-benchmark-config-XXXX.conf)
CONFIG_TARGET=$(mktemp /tmp/nodetype-benchmark-config-target-XXXX.conf)
CONFIG_SOURCE=$(mktemp /tmp/nodetype-benchmark-config-source-XXXX.conf)

# Initialize counter
COUNT=0


######################################
# START OF LOOPS THROUGH CONFIGS #####
######################################

echo ${CONFIG_FILES[0]}
echo ${CONFIG_FILES[1]}

for NODETYPE in "${NODETYPES[@]}"
do

    ######################################
    # CREATE PATH CONFIG FILES ###########
    ######################################
    
    # Set target and source config file, which is the same for both runs
cat > ${CONFIG_SOURCE} <<EOF
@include "${CONFIG//\/tmp\/}"

paths = (
    {
        in = "siggen",
        out = ("source_node", "results_in"),
    }
)
EOF

cat > ${CONFIG_TARGET} <<EOF
@include "${CONFIG//\/tmp\/}"

paths = (
    {
        in = "target_node",
        out = "results_out",

        original_sequence_no = true
    }
)
EOF
    ######################################
    # SPECIAL TREATMENT FOR SOME NODES ###
    ######################################
    
    # Some nodes require special treatment:
    #   * loopback node: target_node is identical to source_node
    #   * infiniband node: one node must be executed in a namespace
    
    # loopback
    if [ "${NODETYPE}" == "loopback" ]; then 
cat > ${CONFIG_TARGET} <<EOF
@include "${CONFIG//\/tmp\/}"

paths = (
    {
        in = "siggen",
        out = ("source_node", "results_in"),
    },
    {
        in = "source_node",
        out = "results_out",

        original_sequence_no = true
    }
)
EOF
    fi
    
    # infiniband
    if [ "${NODETYPE}" == "infiniband" ]; then 
        NAMESPACE_CMD='ip netns exec namespace0'
    else
        NAMESPACE_CMD=''
    fi


    ######################################
    # RUN THROUGH MODES ##################
    ######################################
    for IB_MODE in "${IB_MODES[@]}"
    do
        LOG_DIR=$(date +%Y%m%d_%H-%M-%S)_benchmark_${NODETYPE}_${IB_MODE}
    
        for NUM_VALUE in "${NUM_VALUES[@]}"
        do
            for RATE_SAMPLE in "${RATE_SAMPLES[@]}"
            do
                NUM_SAMPLE=$((${RATE_SAMPLE} * ${TIME_TO_RUN}))
                #TIME_TO_RUN=$((${NUM_SAMPLE} / ${RATE_SAMPLE}))
    
                echo "########################################################"
                echo "########################################################"
                echo "## START ${IB_MODE}"
                echo "## NUM_VALUES: ${NUM_VALUE}"
                echo "## RATE_SAMPLES: ${RATE_SAMPLE}"
                echo "## NUM_SAMPLES: ${NUM_SAMPLE}"
                echo "########################################################"
                echo "########################################################"
    
                # Set wrapper of config file
cat > ${CONFIG} <<EOF
logging = {
    level = 0,
    facilities = "all",
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

        monitor_missed = false
    },

    results_in = {
        type = "file",

        format = "csv",
        uri = "${LOG_DIR}/$(printf "%03d" ${COUNT})_${IB_MODE}-${NUM_VALUE}-${RATE_SAMPLE}-${NUM_SAMPLE}_input.csv",

        buffer_size = 500000000
    },

    results_out = {
        type = "file",

        format = "csv",
        uri = "${LOG_DIR}/$(printf "%03d" ${COUNT})_${IB_MODE}-${NUM_VALUE}-${RATE_SAMPLE}-${NUM_SAMPLE}_output.csv",

        buffer_size = 500000000
    },
EOF

                cat configs/${NODETYPE}.conf | sed -e "s/\${NUM_VALUE}/${NUM_VALUE}/" -e "s/\${IB_MODE}/${IB_MODE}/" >> ${CONFIG} 

cat >> ${CONFIG} <<EOF
}
EOF

                    # Start receiving node
                    VILLAS_LOG_PREFIX=$(colorize "[Target Node]  ") \
                    cset proc --set=real-time-0 --exec ../../build/src/villas-node -- ${CONFIG_TARGET} &
                    target_node_proc=$!
                    
                    # Wait for node to complete init
                    sleep 2
                
                if [ ! "${NODETYPE}" == "loopback" ]; then 
                    # Start sending pipe
                    VILLAS_LOG_PREFIX=$(colorize "[Source Node]  ") \
                    ${NAMESPACE_CMD} cset proc --set=real-time-1 --exec ../../build/src/villas-node -- ${CONFIG_SOURCE} &
                    source_node_proc=$!
                fi
                    
                sleep $((${TIME_TO_RUN} + 5))
                
                # Stop node
                kill $target_node_proc
                
                sleep 1
    
                echo "########################################################"
                echo "## STOP ${IB_MODE}-${NUM_VALUE}-${RATE_SAMPLE}-${NUM_SAMPLE}"
                echo "########################################################"
                echo ""
    
                ((COUNT++))
    
                sleep 1
            done
        done
    
        # Since this script will be executed as sudo we should chmod the
        # log dir 777. Otherwise too many unnecessary 'sudo rm -rf' will be invoked.
        chmod -R 777 ${LOG_DIR}
    done
done

rm ${CONFIG} ${CONFIG_TARGET} ${CONFIG_SOURCE} 

exit 0
