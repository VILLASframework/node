#!/usr/bin/env bash
#
# Integration loopback test using villas node.
#
# This test checks if a single node can be used as an input
# in multiple paths.
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
