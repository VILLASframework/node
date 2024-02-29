#!/usr/bin/env bash
#
# Integration loopback test for villas pipe.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

if ! modprobe -aqn sch_prio sch_netem cls_fw; then
	echo "Netem / TC kernel modules are missing"
	exit 99
fi

if [[ "${EUID}" -ne 0 ]]; then
	echo "Test requires root permissions"
	exit 99
fi

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
			"type"   : "socket",
			"format": "protobuf",

			"in": {
				"address": "*:12000"
			},
			"out": {
				"address": "127.0.0.1:12000",
				"netem": {
					"enabled": true,
					"delay": 100000,
					"jitter": 30000,
					"loss": 20
				}
			}
		}
	}
}
EOF

villas signal -l ${NUM_SAMPLES} -n random > input.dat

villas pipe -l ${NUM_SAMPLES} config.json node1 > output.dat < input.dat

# Check network emulation characteristics
villas hook -o verbose=true -o format=json stats < output.dat > /dev/null
