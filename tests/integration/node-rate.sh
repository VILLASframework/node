#!/bin/bash
#
# Integration loopback test using villas node.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

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
