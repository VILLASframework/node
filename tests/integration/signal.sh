#!/bin/bash
#
# Integration test for villas signal tool
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

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

cat > input.dat <<EOF
# seconds.nanoseconds(sequence)	sine
1635933135.006234199(0)	0.00000000000000000
1635933135.106234199(1)	0.58778525229247314
1635933135.206234199(2)	0.95105651629515353
1635933135.306234199(3)	0.95105651629515353
1635933135.406234199(4)	0.58778525229247325
1635933135.506234199(5)	0.00000000000000012
1635933135.606234199(6)	-0.58778525229247336
1635933135.706234199(7)	-0.95105651629515353
1635933135.806234199(8)	-0.95105651629515364
1635933135.906234199(9)	-0.58778525229247336
EOF

villas signal sine -nl 10 > output.dat

villas compare -T input.dat output.dat
