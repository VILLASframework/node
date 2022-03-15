#!/bin/bash
#
# Integration test for villas compare.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

cat > input.dat <<EOF
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

villas compare input.dat input.dat
(( $? == 0 )) || exit 9

head -n-1 input.dat > temp.dat
villas compare input.dat temp.dat
(( $? == 1 )) || exit 2

( head -n-1 input.dat; echo "1491095597.545159701(55)	-0.587785" ) > temp.dat
villas compare input.dat temp.dat
(( $? == 2 )) || exit 3

( head -n-1 input.dat; echo "1491095598.545159701(9)	-0.587785" ) > temp.dat
villas compare input.dat temp.dat
(( $? == 3 )) || exit 4

( head -n-1 input.dat; echo "1491095597.545159701(9)	-0.587785 -0.587785" ) > temp.dat
villas compare input.dat temp.dat
(( $? == 4 )) || exit 5

( head -n-1 input.dat; echo "1491095597.545159701(9)	-1.587785" ) > temp.dat
villas compare input.dat temp.dat
(( $? == 5 )) || exit 6

( cat input.dat; echo "1491095597.545159701(9)	-0.587785" ) > temp.dat
villas compare input.dat temp.dat
(( $? == 1 )) || exit 7
