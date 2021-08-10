#!/bin/bash
#
# Integration loopback test using villas node.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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
1637846259.201578969(0) 1234.00000000000000000  5678.00000000000000000
1637846259.201578969(1) 1234.00000000000000000  5678.00000000000000000
1637846259.201578969(2) 1234.00000000000000000  5678.00000000000000000
EOF

cat > config.json <<EOF
{
	"nodes": {
		"node_1": {
			"type": "socket",

			"in": {
				"address": ":12000",

				"signals": [
					{ "name": "sig1", "init": 1234.0 },
					{ "name": "sig2", "init": 5678.0 }
				]
			},
			"out": {
				"address": "127.0.0.1:12000"
			}
		},
		"file_1": {
			"type": "file",
			"uri": "output.dat"
		}
	},
	"paths": [
		{
			"in": "node_1",
			"out": "file_1",
			"mode": "all",
			"rate": 10
		}
	]
}
EOF

timeout --preserve-status -k 15s 1s \
villas node config.json

villas compare <(head -n4 < output.dat) expect.dat
