#!/bin/bash
#
# Integration loopback test for villas-pipe.
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

NUM_SAMPLES=${NUM_SAMPLES:-100}

# Generate test data
villas-signal random -l ${NUM_SAMPLES} -n > ${INPUT_FILE}

FORMAT="protobuf"
VECTORIZE="10"

cat > ${CONFIG_FILE} << EOF
{
	"nodes" : {
		"node1" : {
			"type" : "amqp",
			"format" : "${FORMAT}",
			"vectorize" : ${VECTORIZE},

			"uri" : "amqp://localhost",
			"port" : 5672,

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

rabbitmq-server -detached

sleep 5

villas-pipe -l ${NUM_SAMPLES} ${CONFIG_FILE} node1 > ${OUTPUT_FILE} < ${INPUT_FILE}

rabbitmqctl stop

# Compare data
villas-test-cmp ${CMPFLAGS} ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

rm ${OUTPUT_FILE} ${INPUT_FILE} ${CONFIG_FILE}

exit $RC
