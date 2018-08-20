#!/bin/bash
#
# Integration test for remote file IO of file node-type.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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
source ${SCRIPTPATH}/../../tools/integration-tests-helper.sh

CONFIG_FILE=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-10}

URI=https://1Nrd46fZX8HbggT:badpass@rwth-aachen.sciebo.de/public.php/webdav/node/tests/pipe

# WebDav / OwnCloud / Sciebo do not support partial upload
# So we do not flush the output
cat > ${CONFIG_FILE} <<EOF
{
	"nodes" : {
		"remote_file" : {
			"type" : "file",

			"uri" : "${URI}",

			"in" : {
				"epoch_mode" : "original",
				"eof" : "exit"
			},
			"out" : {
				"flush" : false
			}
		}
	}
}
EOF

# Delete old file
curl -sX DELETE ${URI} > /dev/null

# Generate test data
VILLAS_LOG_PREFIX=$(colorize "[Signal] ") \
villas-signal random -n -l ${NUM_SAMPLES} > ${INPUT_FILE}

# Upload data to cloud
VILLAS_LOG_PREFIX=$(colorize "[Send]  ") \
villas-pipe -s ${CONFIG_FILE} remote_file < ${INPUT_FILE}

# Download data from the cloud
VILLAS_LOG_PREFIX=$(colorize "[Recv]  ") \
villas-pipe -r -l ${NUM_SAMPLES} ${CONFIG_FILE} remote_file > ${OUTPUT_FILE}

# Compare results
villas-test-cmp ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

rm -f ${CONFIG_FILE} ${INPUT_FILE} ${OUTPUT_FILE}

exit $RC
