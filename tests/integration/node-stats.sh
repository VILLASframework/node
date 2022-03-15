#!/bin/bash
#
# Integration loopback test using villas node.
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

RATE="33.0"

cat > config.json <<EOF
{
	"nodes": {
		"stats_1": {
			"type": "stats",
			"node": "signal_1",
			"rate": 10.0,
			"in": {
				"signals": [
					{ "name": "gap",   "stats": "signal_1.gap_sent.mean" },
					{ "name": "total", "stats": "signal_1.owd.total" }
				]
			}
		},
		"signal_1": {
			"type": "signal",
			"limit": 100,
			"signal": "sine",
			"rate": ${RATE},
			"in": {
				"hooks": [
					{ "type": "stats", "verbose": true }
				]
			}
		},
		"stats_log_1": {
			"type": "file",
			"format": "json",

			"uri": "stats.json"
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

timeout --preserve-status -k 15s 5s \
villas node config.json

[ -s stats.json ] # check that file exists

tail -n1 stats.json | jq -e "(.data[0] - 1/${RATE} | length) < 1e-4 and .data[1] == 99" > /dev/null
