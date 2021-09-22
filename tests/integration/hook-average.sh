#!/bin/bash
#
# Integration test for average hook.
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
1548104309.033621000(0)	0.022245	0.590769	-1.000000	0.597649	0.100588
1548104309.133998900(1)	0.015339	0.952914	-1.000000	0.196137	0.200966
1548104309.233542500(2)	0.027500	0.950063	-1.000000	-0.202037	0.300509
1548104309.334019400(3)	0.040320	0.582761	-1.000000	-0.603945	0.400986
1548104309.433952200(4)	0.026079	-0.005774	1.000000	-0.996324	0.500919
1548104309.533756400(5)	0.049262	-0.591455	1.000000	-0.597107	0.600723
1548104309.637440300(6)	0.014883	-0.959248	1.000000	-0.182372	0.704407
1548104309.736158700(7)	0.023232	-0.944805	1.000000	0.212502	0.803126
1548104309.833614900(8)	0.015231	-0.584824	1.000000	0.602327	0.900582
1548104309.934288200(9)	0.060849	0.007885	-1.000000	0.994980	0.001255
EOF

cat <<EOF > ${EXPECT_FILE}
# seconds.nanoseconds+offset(sequence)	average	signal0	signal1	signal2	signal3	signal4
1548104309.033621000(0)	0.062250	0.022245	0.590769	-1.000000	0.597649	0.100588
1548104309.133998900(1)	0.073071	0.015339	0.952914	-1.000000	0.196137	0.200966
1548104309.233542500(2)	0.015207	0.027500	0.950063	-1.000000	-0.202037	0.300509
1548104309.334019400(3)	-0.115976	0.040320	0.582761	-1.000000	-0.603945	0.400986
1548104309.433952200(4)	0.104980	0.026079	-0.005774	1.000000	-0.996324	0.500919
1548104309.533756400(5)	0.092285	0.049262	-0.591455	1.000000	-0.597107	0.600723
1548104309.637440300(6)	0.115534	0.014883	-0.959248	1.000000	-0.182372	0.704407
1548104309.736158700(7)	0.218811	0.023232	-0.944805	1.000000	0.212502	0.803126
1548104309.833614900(8)	0.386663	0.015231	-0.584824	1.000000	0.602327	0.900582
1548104309.934288200(9)	0.012994	0.060849	0.007885	-1.000000	0.994980	0.001255
EOF

# Average over first and third signal (mask = 0b101 = 5)
villas hook -o offset=0 -o signals=signal0,signal1,signal2,signal3,signal4 average < ${INPUT_FILE} > ${OUTPUT_FILE}

# Compare only the data values
villas compare ${OUTPUT_FILE} ${EXPECT_FILE}
RC=$?

rm -f ${INPUT_FILE} ${OUTPUT_FILE} ${EXPECT_FILE}

exit ${RC}
