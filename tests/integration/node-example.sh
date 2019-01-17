#!/bin/bash
#
# Test for the example node-type
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
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

echo "The example node does nothing useful which we could test"
exit 99

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

cat > expect.dat <<EOF
1553690684.041211800-4.313770e-02(5)	6.303265	0.492616	0.309017	-1.000000	0.800000	0.050000
1553690684.051211800-5.287260e-02(6)	5.673902	0.148827	0.368125	-1.000000	0.760000	0.060000
1553690684.061211800-6.266780e-02(7)	5.896198	0.232320	0.425779	-1.000000	0.720000	0.070000
1553690684.071211800-7.256350e-02(8)	5.788125	0.152309	0.481754	-1.000000	0.680000	0.080000
1553690684.081211800-8.251890e-02(9)	6.748635	0.608491	0.535827	-1.000000	0.640000	0.090000
EOF

cat > config.json <<EOF
{
	"nodes": {
		"example_node1": {
			"type": "example",

			"setting1": 66
		},
		"file": {
			"type": "file",
			"uri": "output.dat"
		}
	},
	"paths": [
		{
			"in": "example_node1",
			"out": "file",
		}
	]
}
EOF

villas node config.json

villas compare output.dat expect.dat