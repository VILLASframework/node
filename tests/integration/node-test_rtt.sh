#!/bin/bash
#
# Integration loopback test using villas node.
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

echo "Test not yet supported"
exit 99

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

mkdir ./logs

cat > config.json <<EOF
{
	"logging": { "level": 2 },
	"stats": 1,
	"nodes": {
		"test": {
			"type": "test_rtt",
			"cooldown": 2,
			"output": "./logs",
			"cases": [
				{
					"rates": 55.0,
					"values": 5,
					"limit": 100
				},
				{
					"rates": [ 10, 10, 1000 ],
					"values": [ 2, 10, 20, 50 ],
					"duration": 5
				}
			],
			"hooks": [
				{
					"type": "stats",
					"verbose": false
				}
			]
		}
	},
	"paths": [
		{
			"in": "test",
			"out": "test"
		}
	]
}
EOF

villas node config.json
