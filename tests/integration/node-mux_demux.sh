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
OUTPUT_FILE=$(mktemp)
EXPECT_FILE=$(mktemp)

cat <<EOF > ${EXPECT_FILE}
1599745986.343431058+1.451000e-06(7)	4.000000	20.000000	100.000000
1599745986.843081878+8.820000e-07(16)	9.000000	50.000000	200.000000
1599745987.343081848+9.440000e-07(24)	14.000000	70.000000	300.000000
1599745987.843081745+8.630000e-07(33)	19.000000	100.000000	400.000000
EOF

cat > ${CONFIG_FILE} <<EOF
{
	"nodes": {
		"sig_1": {
			"type": "signal",

			"signal" : "counter",
			"values" : 1,
			"offset" : 0.0,
			"rate" : 10.0,
			"limit" : 20
		},
		"sig_2": {
			"type": "signal",

			"signal" : "counter",
			"values" : 1,
			"offset" : 10.0,
			"amplitude" : 10.0,
			"rate" : 5.0,
			"limit" : 10
		},
		"sig_3": {
			"type": "signal",

			"signal" : "counter",
			"values" : 1,
			"offset" : 100.0,
			"amplitude" : 100.0,
			"rate" : 2.0,
			"limit" : 10
		},
		"file_1": {
			"type": "file",
			"uri" : "${OUTPUT_FILE}"
		}
	},
	"paths": [
		{
			"in": [
				"sig_1.data[counter]",
				"sig_2.data[counter]",
				"sig_3.data[counter]",
				"sig_1.hdr.length",
				"sig_1.hdr.sequence",
				"sig_1.ts.origin"
			],
			"out": "file_1",
			"mode" : "all"
		}
	]
}
EOF

# Start node
VILLAS_LOG_PREFIX=$(colorize "[Node]  ") \
villas-node ${CONFIG_FILE}

# Compare only the data values
villas-test-cmp ${OUTPUT_FILE} ${EXPECT_FILE}
RC=$?

rm ${CONFIG_FILE} ${OUTPUT_FILE} ${EXPECT_FILE}

exit ${RC}
