#!/bin/bash
#
# Integration loopback test using villas-node.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})
source ${SCRIPTPATH}/../../tools/integration-tests-helper.sh

CONFIG_FILE=$(mktemp)

cat > ${CONFIG_FILE} <<EOF
{
	"stats": 1.0,
	"nodes": {
		"stats_1": {
			"type": "stats",
			"node": "signal_1",
			"rate": 1.0
		},
		"signal_1": {
			"type": "signal",
			"limit": 100,
			"rate": 10.0,
			"in" : {
				"hooks": [
					{ "type": "stats", "verbose": true }
				]
			}
		}
	},
	"paths": [
		{
			"in": "signal_1",
			"hooks" : [
				{ "type" : "print", "enabled" : false }
			]
		},
		{
			"in": "stats_1",
			"hooks" : [
				{ "type" : "print" }
			]
		}
	]
}
EOF

# Start node
VILLAS_LOG_PREFIX=$(colorize "[Node]  ") \
villas-node ${CONFIG_FILE}
