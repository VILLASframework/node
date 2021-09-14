#!/bin/bash
#
# Integration test for gate hook.
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

INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)
EXPECT_FILE=$(mktemp)

cat <<EOF > ${INPUT_FILE}
# seconds.nanoseconds(sequence)	random	sine	square	triangle	ramp
1561591854.277376100(0)	0.022245	-0.136359	1.000000	-0.912920	0.104354
1561591854.377354600(1)	0.015339	0.135690	-1.000000	0.913350	0.204333
1561591854.475834300(2)	0.027500	-0.088233	1.000000	-0.943756	0.302812
1561591854.575995900(3)	0.040320	0.093289	-1.000000	0.940524	0.402974
1561591854.675955300(4)	0.026079	-0.092019	1.000000	-0.941336	0.502933
1561591854.776188900(5)	0.049262	0.099324	-1.000000	0.936664	0.603167
1561591854.875119100(6)	0.014883	-0.065832	1.000000	-0.958060	0.702097
1561591854.974264200(7)	0.023232	0.039012	-1.000000	0.975158	0.801242
1561591855.077804200(8)	0.015231	-0.149670	1.000000	-0.904358	0.904782
1561591855.174828300(9)	0.060849	0.056713	-1.000000	0.963876	1.001806
EOF

cat <<EOF > ${EXPECT_FILE}
# seconds.nanoseconds+offset(sequence)	signal0	signal1	signal2	signal3	signal4
1561591854.277376100(0)	0.022245	-0.136359	1.000000	-0.912920	0.104354
1561591854.475834300(2)	0.027500	-0.088233	1.000000	-0.943756	0.302812
1561591854.675955300(4)	0.026079	-0.092019	1.000000	-0.941336	0.502933
1561591854.875119100(6)	0.014883	-0.065832	1.000000	-0.958060	0.702097
1561591855.077804200(8)	0.015231	-0.149670	1.000000	-0.904358	0.904782
EOF

villas-hook gate -o signal=signal2 -o mode=above < ${INPUT_FILE} > ${OUTPUT_FILE}

# Compare only the data values
villas-compare ${OUTPUT_FILE} ${EXPECT_FILE}
RC=$?

rm -f ${INPUT_FILE} ${OUTPUT_FILE} ${EXPECT_FILE}

exit ${RC}
