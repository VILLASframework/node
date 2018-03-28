#!/bin/bash
#
# Integration loopback test for villas-pipe.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

# We skip this test for now, as we dont have a AMQP broker in the CI environment
exit 99

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})
source ${SCRIPTPATH}/../../tools/integration-tests-helper.sh

CONFIG_FILE=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-100}

# Generate test data
villas-signal random -l ${NUM_SAMPLES} -n > ${INPUT_FILE}

for FORMAT in villas-human villas-binary villas-web csv json gtnet-fake raw-flt32; do
	
VECTORIZES="1"

# The raw format does not support vectors
if villas_format_supports_vectorize ${FORMAT}; then
	VECTORIZES="${VECTORIZES} 10"
fi

for VECTORIZE	in ${VECTORIZES}; do	

cat > ${CONFIG_FILE} << EOF
{
	"nodes" : {
		"node1" : {
			"type" : "amqp",
			"format" : "${FORMAT}",
			"vectorize" : ${VECTORIZE},

			"uri" : "amqp://broker",

			"exchange" : "mytestexchange",
			"routing_key" : "abc",
		
			"ssl" : {
				"verify_hostname" : true,
				"verify_peer" : true,

				"ca_cert" : "/path/to/ca.crt",
				"client_cert" : "/path/to/client.crt",
				"client_key" : "/path/to/client.key"
			}
		}
	}
}
EOF

villas-pipe -l ${NUM_SAMPLES} ${CONFIG_FILE} node1 > ${OUTPUT_FILE} < ${INPUT_FILE}

# Ignore timestamp and seqeunce no if in raw format 
if villas_format_supports_header ${FORMAT}; then
	CMPFLAGS=-ts
fi

# Compare data
villas-test-cmp ${CMPFLAGS} ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

if (( ${RC} != 0 )); then
	echo "=========== Sub-test failed for: format=${FORMAT}, vectorize=${VECTORIZE}"
	cat ${CONFIG_FILE}
	echo
	cat ${INPUT_FILE}
	echo
	cat ${OUTPUT_FILE}
	exit ${RC}
else
	echo "=========== Sub-test succeeded for: format=${FORMAT}, vectorize=${VECTORIZE}"
fi

done; done

rm ${OUTPUT_FILE} ${INPUT_FILE} ${CONFIG_FILE}

exit $RC
