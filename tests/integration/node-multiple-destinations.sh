#!/bin/bash
#
# Integration loopback test using villas node.
#
# This test checks if a single node can be used as an input
# in multiple paths.
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
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
