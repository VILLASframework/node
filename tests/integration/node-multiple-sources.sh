#!/bin/bash
#
# Integration loopback test using villas node.
#
# This test checks if a single node can be used as an input
# in multiple paths.
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

echo "Test is broken"
exit 99

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

cat > config.json <<EOF
{
		"nodes": {
				"sig_1": {
						"type": "signal",
						"values": 1,
						"signal": "counter",
						"offset": 100,
						"limit": 10,
						"realtime": false
				},
				"file_1": {
						"type": "file",
						"uri": "output1.dat"
				},
				"file_2": {
						"type": "file",
						"uri": "output2.dat"
				}
		},
		"paths": [
			{
				"in": "sig_1",
				"out": "file_1"
			},
			{
				"in": "sig_1",
				"out": "file_2"
			},
			{
				"in": "sig_1"
			},
			{
				"in": "sig_1"
			}
		]
}
EOF

villas signal counter -o 100 -v 1 -l 10 -n > expect.dat

timeout --preserve-status -k 15s 1s \
villas node config.json

villas compare output1.dat expect.dat && \
villas compare output2.dat expect.dat
