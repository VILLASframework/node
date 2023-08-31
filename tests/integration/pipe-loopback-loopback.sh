#!/bin/bash
#
# Integration loopback test for villas pipe.
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

NUM_SAMPLES=${NUM_SAMPLES:-10}

cat > config.json << EOF
{
	"nodes": {
		"node1": {
			"type": "loopback"
		}
	}
}
EOF

villas signal -v 5 -l ${NUM_SAMPLES} -n mixed > input.dat

villas pipe -l ${NUM_SAMPLES} config.json node1 > output.dat < input.dat

villas compare input.dat output.dat
