#!/bin/bash
#
# Integration loopback test using villas-node.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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
source ${SCRIPTPATH}/../../tools/villas-helper.sh

CONFIG_FILE=$(mktemp)
STATS_LOG=$(mktemp)

RATE="33.0"

cat > ${CONFIG_FILE} <<EOF
{
	"nodes": {
		"stats_1": {
			"type": "stats",
			"node": "signal_1",
			"rate": 10.0,
			"in": {
				"signals": [
					{ "name": "gap", "stats": "signal_1.gap_sent.mean" },
					{ "name": "total", "stats": "signal_1.owd.total" }
				]
			}
		},
		"signal_1": {
			"type": "signal",
			"limit": 100,
			"rate": ${RATE},
			"in" : {
				"hooks": [
					{ "type": "stats", "verbose": true }
				]
			}
		},
		"stats_log_1": {
			"type": "file",
			"format": "json",

			"uri": "${STATS_LOG}"
		}
	},
	"paths": [
		{
			"in": "signal_1"
		},
		{
			"in": "stats_1",
			"out": "stats_log_1"
		}
	]
}
EOF

# Start node
VILLAS_LOG_PREFIX=$(colorize "[Node]  ") \
villas-node ${CONFIG_FILE} &
PID=$!

sleep 5

kill ${PID}

tail -n1 ${STATS_LOG} | jq -e "(.data[0] - 1/${RATE} | length) < 1e-4 and .data[1] == 99" > /dev/null
RC=$?

rm ${STATS_LOG} ${CONFIG_FILE}

exit ${RC}
