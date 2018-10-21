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

CONFIG_FILE=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-10}

for SAMPLELEN	in 1 10 100; do
for POLLING	in true false; do
for VECTORIZE	in 1 5 25; do

cat > ${CONFIG_FILE} << EOF
{
	"logging" : {
		"level" : 2
	},
	"nodes" : {
		"node1" : {
			"type" : "shmem",
			"out" : {
				"name" : "/villas-test"
			},
			"in" : {
				"name" : "/villas-test"	
			},
			"samplelen" : ${SAMPLELEN},
			"queuelen" : 1024,
			"polling" : ${POLLING},
			"vectorize" : ${VECTORIZE}
		}
	}
}
EOF

# Generate test data
villas-signal -l ${NUM_SAMPLES} -v ${SAMPLELEN} -n random > ${INPUT_FILE}

villas-pipe -l ${NUM_SAMPLES} ${CONFIG_FILE} node1 > ${OUTPUT_FILE} < ${INPUT_FILE}

# Compare data
villas-test-cmp ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

if (( ${RC} != 0 )); then
	echo "=========== Sub-test failed for: polling=${POLLING}, vectorize=${VECTORIZE}, samplelen=${SAMPLELEN}"
	cat ${CONFIG_FILE}
	echo
	cat ${INPUT_FILE}
	echo
	cat ${OUTPUT_FILE}
	exit ${RC}
else
	echo "=========== Sub-test succeeded for: polling=${POLLING}, vectorize=${VECTORIZE}, samplelen=${SAMPLELEN}"
fi

done; done; done

rm ${OUTPUT_FILE} ${INPUT_FILE} ${CONFIG_FILE}

exit $RC
