#!/bin/bash
#
# Integration loopback test for villas-pipe.
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

if ! modprobe -aqn sch_prio sch_netem cls_fw; then
	echo "Netem / TC kernel modules are missing"
	exit 99
fi

CONFIG_FILE=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-10}

cat > ${CONFIG_FILE} << EOF
{
	"nodes" : {
		"node1" : {
			"type"   : "socket",
			"format" : "protobuf",

			"in" : {
				"address" : "*:12000"
			},
			"out" : {
				"address" : "127.0.0.1:12000",
				"netem" : {
					"enabled" : true,
					"delay" : 100000,
					"jitter" : 30000,
					"loss" : 20
				}
			}
		}
	}
}
EOF

# Generate test data
villas-signal -l ${NUM_SAMPLES} -n random > ${INPUT_FILE}

villas-pipe -l ${NUM_SAMPLES} ${CONFIG_FILE} node1 > ${OUTPUT_FILE} < ${INPUT_FILE}

# Check network emulation characteristics
villas-hook -o verbose stats < ${OUTPUT_FILE} > /dev/null

rm ${OUTPUT_FILE} ${INPUT_FILE} ${CONFIG_FILE}
