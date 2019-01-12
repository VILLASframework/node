#!/bin/bash
#
# Integration loopback test for villas-pipe.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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
THEORIES=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-100}
NUM_VALUES=${NUM_VALUES:-4}

# Generate test data
villas-signal -v ${NUM_VALUES} -l ${NUM_SAMPLES} -n random > ${INPUT_FILE}

for FORMAT in villas.human villas.binary villas.web csv json gtnet.fake raw.32.le raw.64.be protobuf; do
for LAYER in udp ip eth unix; do
for VERIFY_SOURCE in true false; do
	
VECTORIZES="1"

# The raw format does not support vectors
if villas_format_supports_vectorize ${FORMAT}; then
	VECTORIZES="${VECTORIZES} 10"
fi

for VECTORIZE in ${VECTORIZES}; do

case ${LAYER} in
	udp)
		LOCAL="127.0.0.1:12000"
		REMOTE="127.0.0.1:12000"
		;;

	ip)
		# We use IP protocol number 253 which is reserved for experimentation and testing according to RFC 3692
		LOCAL="127.0.0.1:254"
		REMOTE="127.0.0.1:254"
		;;

	eth)
		LOCAL="00:00:00:00:00:00%lo:34997"
		REMOTE="00:00:00:00:00:00%lo:34997"
		;;
		
	unix)
		LOCAL=$(mktemp)
		REMOTE=${LOCAL}
		;;
esac

cat > ${CONFIG_FILE} << EOF
{
	"nodes" : {
		"node1" : {
			"type" : "socket",
			
			"vectorize" : ${VECTORIZE},
			"format" : "${FORMAT}",
			"layer" : "${LAYER}",

			"out" : {
				"address" : "${REMOTE}"
			},
			"in" : {
				"address" : "${LOCAL}",
				"verify_source" : ${VERIFY_SOURCE},
				"signals" : {
					"count" : ${NUM_VALUES},
					"type" : "float"
				}
			}
		}
	}
}
EOF

villas-pipe -l ${NUM_SAMPLES} ${CONFIG_FILE} node1 < ${INPUT_FILE} > ${OUTPUT_FILE}

# Ignore timestamp and seqeunce no if in raw format 
if ! villas_format_supports_header $FORMAT; then
	CMPFLAGS=-ts
fi

# Compare data
villas-test-cmp ${CMPFLAGS} ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

if (( ${RC} != 0 )); then
	echo "=========== Sub-test failed for: format=${FORMAT}, layer=${LAYER}, verify_source=${VERIFY_SOURCE}, vectorize=${VECTORIZE}"
	echo "Config:"
	cat ${CONFIG_FILE}
	echo
	echo "Input:"
	cat ${INPUT_FILE}
	echo
	echo "Output:"
	cat ${OUTPUT_FILE}
	exit ${RC}
else
	echo "=========== Sub-test succeeded for: format=${FORMAT}, layer=${LAYER}, verify_source=${VERIFY_SOURCE}, vectorize=${VECTORIZE}"
fi

done; done; done; done

rm ${OUTPUT_FILE} ${INPUT_FILE} ${CONFIG_FILE} ${THEORIES}

exit $RC
