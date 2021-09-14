#!/bin/bash
#
# Integration test for villas-compare.
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
TEMP_FILE=$(mktemp)

function fail() {
	rm ${INPUT_FILE} ${TEMP_FILE}
	exit $1;
}

cat > ${INPUT_FILE} <<EOF
1491095596.645159701(0)	0.000000
1491095596.745159701(1)	0.587785
1491095596.845159701(2)	0.951057
1491095596.945159701(3)	0.951057
1491095597.045159701(4)	0.587785
1491095597.145159701(5)	0.000000
1491095597.245159701(6)	-0.587785
1491095597.345159701(7)	-0.951057
1491095597.445159701(8)	-0.951057
1491095597.545159701(9)	-0.587785
EOF

villas-compare ${INPUT_FILE} ${INPUT_FILE}
(( $? == 0 )) || fail 1

head -n-1 ${INPUT_FILE} > ${TEMP_FILE}
villas-compare ${INPUT_FILE} ${TEMP_FILE}
(( $? == 1 )) || fail 2

( head -n-1 ${INPUT_FILE}; echo "1491095597.545159701(55)	-0.587785" ) > ${TEMP_FILE}
villas-compare ${INPUT_FILE} ${TEMP_FILE}
(( $? == 2 )) || fail 3

( head -n-1 ${INPUT_FILE}; echo "1491095598.545159701(9)	-0.587785" ) > ${TEMP_FILE}
villas-compare ${INPUT_FILE} ${TEMP_FILE}
(( $? == 3 )) || fail 4

( head -n-1 ${INPUT_FILE}; echo "1491095597.545159701(9)	-0.587785 -0.587785" ) > ${TEMP_FILE}
villas-compare ${INPUT_FILE} ${TEMP_FILE}
(( $? == 4 )) || fail 5

( head -n-1 ${INPUT_FILE}; echo "1491095597.545159701(9)	-1.587785" ) > ${TEMP_FILE}
villas-compare ${INPUT_FILE} ${TEMP_FILE}
(( $? == 5 )) || fail 6

( cat ${INPUT_FILE}; echo "1491095597.545159701(9)	-0.587785" ) > ${TEMP_FILE}
villas-compare ${INPUT_FILE} ${TEMP_FILE}
(( $? == 1 )) || fail 7

rm ${INPUT_FILE} ${TEMP_FILE}
