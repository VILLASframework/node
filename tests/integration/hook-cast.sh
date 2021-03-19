#!/bin/bash
#
# Integration test for cast hook.
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

# Test is broken
exit 99

INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)
EXPECT_FILE=$(mktemp)

cat <<EOF > ${INPUT_FILE}
# seconds.nanoseconds+offset(sequence)	signal0	signal1	signal2	signal3	signal4
1551015508.801653200(0)	0.022245	0.000000	-1.000000	1.000000	0.000000
1551015508.901653200(1)	0.015339	58.778500	-1.000000	0.600000	0.100000
1551015509.001653200(2)	0.027500	95.105700	-1.000000	0.200000	0.200000
1551015509.101653200(3)	0.040320	95.105700	-1.000000	-0.200000	0.300000
1551015509.201653200(4)	0.026079	58.778500	-1.000000	-0.600000	0.400000
1551015509.301653200(5)	0.049262	0.000000	1.000000	-1.000000	0.500000
1551015509.401653200(6)	0.014883	-58.778500	1.000000	-0.600000	0.600000
1551015509.501653200(7)	0.023232	-95.105700	1.000000	-0.200000	0.700000
1551015509.601653200(8)	0.015231	-95.105700	1.000000	0.200000	0.800000
1551015509.701653200(9)	0.060849	-58.778500	1.000000	0.600000	0.900000
EOF

cat <<EOF > ${EXPECT_FILE}
# seconds.nanoseconds+offset(sequence)	signal0	test[V]	signal2	signal3	signal4
1551015508.801653200(0)	0.022245	0	-1.000000	1.000000	0.000000
1551015508.901653200(1)	0.015339	58	-1.000000	0.600000	0.100000
1551015509.001653200(2)	0.027500	95	-1.000000	0.200000	0.200000
1551015509.101653200(3)	0.040320	95	-1.000000	-0.200000	0.300000
1551015509.201653200(4)	0.026079	58	-1.000000	-0.600000	0.400000
1551015509.301653200(5)	0.049262	0	1.000000	-1.000000	0.500000
1551015509.401653200(6)	0.014883	-58	1.000000	-0.600000	0.600000
1551015509.501653200(7)	0.023232	-95	1.000000	-0.200000	0.700000
1551015509.601653200(8)	0.015231	-95	1.000000	0.200000	0.800000
1551015509.701653200(9)	0.060849	-58	1.000000	0.600000	0.900000
EOF

villas-hook cast -o new_name=test -o new_unit=V -o new_type=integer -o signal=1 < ${INPUT_FILE} > ${OUTPUT_FILE}

# Compare only the data values
villas-test-cmp ${OUTPUT_FILE} ${EXPECT_FILE}
RC=$?

rm -f ${INPUT_FILE} ${OUTPUT_FILE} ${EXPECT_FILE}

exit ${RC}
