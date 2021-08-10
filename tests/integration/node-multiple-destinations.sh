#!/bin/bash
#
# Integration loopback test using villas node.
#
# This test checks if a single node can be used as an input
# in multiple paths.
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
set -x

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
						"limit": 10
				},
				"file_1": {
						"type": "file",
						"uri": "output1.dat"
				}
		},
		"paths": [
			{
				"in": "sig_1",
				"out": "file_1"
			},
			{
				"in": "sig_1",
				"out": "file_1"
			}
		]
}
EOF

# This configuration is invalid and must fail
! villas node config.json 2> error.log

# Error log must include description
grep -q "Every node must only be used by a single path as destination" error.log
