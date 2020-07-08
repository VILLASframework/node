#!/bin/bash
#
# Integration can test using villas-node.
#
# @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
# @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})
source ${SCRIPTPATH}/../../tools/villas-helper.sh

CONFIG_FILE=${SCRIPTPATH}/../../etc/examples/nodes/can.conf
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)
CAN_OUT_FILE=$(mktemp)
CAN_IF=vcan0

NUM_SAMPLES=${NUM_SAMPLES:-10}
NUM_VALUES=${NUM_VALUES:-3}

# Check if vcan0 interface is present
if [[ ! $(ip link show "${CAN_IF}" up) ]]; then
    echo "Did not find any vcan interface ${CAN_IF} or interface is not up"
    exit 99
fi

#sudo modprobe vcan
#sudo ip link add dev vcan0 type vcan
#sudo ip link set vcan0 up

# Generate test data
VILLAS_LOG_PREFIX=$(colorize "[Signal]") \
villas-signal -v ${NUM_VALUES} -l ${NUM_SAMPLES} -n random > ${INPUT_FILE}

# Start node
VILLAS_LOG_PREFIX=$(colorize "[Node]  ") \
villas-node ${CONFIG_FILE} &

# Wait for node to complete init
sleep 1

candump -n ${NUM_SAMPLES} -T 1000 -L ${CAN_IF} | awk '{print $3}' > ${CAN_OUT_FILE} &
CANDUMP_PID=$!

# Send / Receive data to node
VILLAS_LOG_PREFIX=$(colorize "[Pipe]  ") \
villas-pipe -l ${NUM_SAMPLES} ${CONFIG_FILE} can_node1 > ${OUTPUT_FILE} < ${INPUT_FILE} &

wait ${CANDUMP_PID}
cat ${CAN_OUT_FILE} | xargs -I % cansend ${CAN_IF} %

# Wait for node to handle samples
sleep 1

# Stop node
kill %1

# Compare data
villas-test-cmp ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

#rm ${CAN_OUT_FILE} ${INPUT_FILE} ${OUTPUT_FILE}
rm ${INPUT_FILE} ${OUTPUT_FILE}

exit $RC
