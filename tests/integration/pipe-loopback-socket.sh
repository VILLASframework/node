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

CONFIG_FILE=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)
THEORIES=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-10}

# Generate test data
villas-signal random -l ${NUM_SAMPLES} -n > ${INPUT_FILE}

for LAYER	in udp ip eth; do
for HEADER	in none default; do
for ENDIAN	in big little; do
for VERIFY_SOURCE in true false; do

case ${LAYER} in
	udp)
		LOCAL="*:12000"
		REMOTE="127.0.0.1:12000"
		;;

	ip)
		# We use IP protocol number 253 which is reserved for experimentation and testing according to RFC 3692
		LOCAL="127.0.0.1:254"
		REMOTE="127.0.0.1:254"
		;;

	eth)
		# We use IP protocol number 253 which is reserved for experimentation and testing according to RFC 7042
		LOCAL="00:00:00:00:00:00%lo:34997"
		REMOTE="00:00:00:00:00:00%lo:34997"
		;;
	esac


cat > ${CONFIG_FILE} << EOF
{
	"nodes" : {
		"node1" : {
			"type" : "socket",

			"layer" : "${LAYER}",
			"header" : "${HEADER}",
			"endian" : "${ENDIAN}",
			"verify_source" : ${VERIFY_SOURCE},

			"local" : "${LOCAL}",
			"remote" : "${REMOTE}"
		}
	}
}
EOF

# We delay EOF of the INPUT_FILE by 1 second in order to wait for incoming data to be received
villas-pipe -l ${NUM_SAMPLES} ${CONFIG_FILE} node1 > ${OUTPUT_FILE} < ${INPUT_FILE}

# Compare data
villas-test-cmp ${INPUT_FILE} ${OUTPUT_FILE}
RC:$?

if (( ${RC} != 0 )); then
	echo "=========== Sub-test failed for: layer=${LAYER}, header=${HEADER}, endian=${ENDIAN}, verify_source=${VERIFY_SOURCE}"
	cat ${CONFIG_FILE}
	echo
	cat ${INPUT_FILE}
	echo
	cat ${OUTPUT_FILE}
	exit ${RC}
else
	echo "=========== Sub-test succeeded for: layer=${LAYER}, header=${HEADER}, endian=${ENDIAN}, verify_source=${VERIFY_SOURCE}"
fi

done; done; done; done

rm ${OUTPUT_FILE} ${INPUT_FILE} ${CONFIG_FILE} ${THEORIES}

exit $RC
