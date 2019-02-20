#!/bin/bash
#
# Integration loopback test using villas-node.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

# We skip this test for now
echo "Test not yet supported"
exit 99

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})
source ${SCRIPTPATH}/../../tools/villas-helper.sh

CONFIG_FILE=$(mktemp)

cat > ${CONFIG_FILE} <<EOF
{
	"logging": { "level": 2 },
	"stats": 1,
	"nodes": {
		"test": {
			"type": "test_rtt",
			"cooldown": 2,
			"output" : "${LOGDIR}/test_rtt/",
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
			"hooks" : [
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

# Start node
VILLAS_LOG_PREFIX=$(colorize "[Node]  ") \
villas-node ${CONFIG_FILE}
