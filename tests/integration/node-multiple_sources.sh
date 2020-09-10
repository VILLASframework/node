#!/bin/bash
#
# Integration loopback test using villas-node.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

CONFIG_FILE=$(mktemp)
OUTPUT_FILE1=$(mktemp)
OUTPUT_FILE2=$(mktemp)
EXPECT_FILE=$(mktemp)

cat > ${CONFIG_FILE} <<EOF
{
  "nodes": {
    "sig_1": {
      "type": "signal",
      "values": 2,
      "signal": "counter",
      "offset": 100,
      "limit": 10
    },
    "file_1": {
        "type": "file",
        "uri": "${OUTPUT_FILE1}"
    },
    "file_2": {
        "type": "file",
        "uri": "${OUTPUT_FILE2}"
    }
  },
  "paths": [
    {
      "in": [
        "sig_1.data[0]"
      ],
      "out": "file_1"
    },
    {
      "in": [
        "sig_1.data[0]"
      ],
      "out": "file_2"
    }
  ]
}
EOF

villas signal counter -o 100 -v 1 -l 10 -n > ${EXPECT_FILE}

# Start node
VILLAS_LOG_PREFIX=$(colorize "[Node]  ") \
villas-node ${CONFIG_FILE} &
P1=$!

sleep 2

kill ${P1}
wait ${P1}

# Compare only the data values
villas-test-cmp ${OUTPUT_FILE1} ${EXPECT_FILE} && \
villas-test-cmp ${OUTPUT_FILE2} ${EXPECT_FILE}
RC=$?

rm ${CONFIG_FILE} ${EXPECT_FILE} \
   ${OUTPUT_FILE1} ${OUTPUT_FILE2}

exit ${RC}
