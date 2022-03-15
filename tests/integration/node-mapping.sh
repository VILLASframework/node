#!/bin/bash
#
# Integration loopback test using villas node.
#
# Test test checks if source mapping features for a path.
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

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

cat > expect.dat <<EOF
1637161831.766626638(1)	99	123	3.00000000000000000	12.00000000000000000
EOF

cat > config.json <<EOF
{
	"nodes": {
		"sig_1": {
			"type": "signal.v2",

			"limit": 1,

			"initial_sequenceno": 99,

			"in": {
				"signals": [
					{ "name": "const1", "signal": "constant", "amplitude": 1 },
					{ "name": "const2", "signal": "constant", "amplitude": 2 },
					{ "name": "const3", "signal": "constant", "amplitude": 3 }
				]
			}
		},
		"sig_2": {
			"type": "signal.v2",

			"limit": 1,

			"initial_sequenceno": 123,

			"in": {
				"signals": [
					{ "name": "const1", "signal": "constant", "amplitude": 11 },
					{ "name": "const2", "signal": "constant", "amplitude": 12 },
					{ "name": "const3", "signal": "constant", "amplitude": 13 }
				]
			}
		},
		"file_1": {
			"type": "file",
			"uri": "output.dat"
		}
	},
	"paths": [
		{
			"in": [
				"sig_1.hdr.sequence",
				"sig_2.hdr.sequence",
				"sig_1.data[const3]",
				"sig_2.data[const2]"
			],
			"out": "file_1",
			"mode": "all"
		}
	]
}
EOF

villas node config.json

villas compare output.dat expect.dat
