#!/bin/bash
#
# Test hooks in villas node
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

CONFIG_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)
EXPECT_FILE=$(mktemp)

cat <<EOF > ${EXPECT_FILE}
1553690684.041211800-4.313770e-02(5)	6.303265	0.492616	0.309017	-1.000000	0.800000	0.050000
1553690684.051211800-5.287260e-02(6)	5.673902	0.148827	0.368125	-1.000000	0.760000	0.060000
1553690684.061211800-6.266780e-02(7)	5.896198	0.232320	0.425779	-1.000000	0.720000	0.070000
1553690684.071211800-7.256350e-02(8)	5.788125	0.152309	0.481754	-1.000000	0.680000	0.080000
1553690684.081211800-8.251890e-02(9)	6.748635	0.608491	0.535827	-1.000000	0.640000	0.090000
EOF

cat > ${CONFIG_FILE} <<EOF
{
	"nodes": {
		"sig": {
			"type": "signal",

			"signal": "mixed",
			"realtime": false,
			"limit": 10,
			"rate": 100,
			"values": 5
		},
		"file": {
			"type": "file",
			"uri": "${OUTPUT_FILE}"
		}
	},
	"paths": [
		{
			"in": "sig",
			"out": "file",
			"hooks": [
				{
					"type": "average",
					
					"signals" : [ "random", "sine", "square", "triangle", "ramp" ],
					"offset": 0
				},
				{
					"type": "skip_first",
					
					"samples": 5
				},
				{
					"type": "scale",

					"scale": 10,
					"offset": 5,
					"signal": "average"
				}
			]
		}
	]
}
EOF

# Start node
villas node ${CONFIG_FILE}

# Compare only the data values
villas compare ${OUTPUT_FILE} ${EXPECT_FILE}
RC=$?

cat ${OUTPUT_FILE}

rm ${CONFIG_FILE} ${OUTPUT_FILE} ${EXPECT_FILE}

exit ${RC}
