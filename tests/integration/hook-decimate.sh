#!/bin/bash
#
# Integration test for decimate hook.
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

INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)
EXPECT_FILE=$(mktemp)

cat <<EOF > ${INPUT_FILE}
1490500399.776379108(0)	0.000000	0.000000	0.000000	0.000000
1490500399.876379108(1)	0.587785	0.587785	0.587785	0.587785
1490500399.976379108(2)	0.951057	0.951057	0.951057	0.951057
1490500400.076379108(3)	0.951057	0.951057	0.951057	0.951057
1490500400.176379108(4)	0.587785	0.587785	0.587785	0.587785
1490500400.276379108(5)	0.000000	0.000000	0.000000	0.000000
1490500400.376379108(6)	-0.587785	-0.587785	-0.587785	-0.587785
1490500400.476379108(7)	-0.951057	-0.951057	-0.951057	-0.951057
1490500400.576379108(8)	-0.951057	-0.951057	-0.951057	-0.951057
1490500400.676379108(9)	-0.587785	-0.587785	-0.587785	-0.587785
EOF

cat <<EOF > ${EXPECT_FILE}
1490500399.776379108(0)	0.000000	0.000000	0.000000	0.000000
1490500400.076379108(3)	0.951057	0.951057	0.951057	0.951057
1490500400.376379108(6)	-0.587785	-0.587785	-0.587785	-0.587785
1490500400.676379108(9)	-0.587785	-0.587785	-0.587785	-0.587785
EOF

villas-hook -o ratio=3 decimate < ${INPUT_FILE} > ${OUTPUT_FILE}

# Compare only the data values
villas-test-cmp ${OUTPUT_FILE} ${EXPECT_FILE}

RC=$?

rm -f ${INPUT_FILE} ${OUTPUT_FILE} ${EXPECT_FILE}

exit $RC
