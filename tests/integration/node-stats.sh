#!/usr/bin/env bash
#
# Integration loopback test using villas node.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

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
