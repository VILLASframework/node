#!/bin/bash
#
# Integration loopback test for villas pipe.
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
##################################################################################

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
				"address": "*:12000",
				
				"multicast": {
					"enabled": true,

					"group"   : "224.1.2.3",
					"loop"    : true
				}
			},
			"out": {
				"address": "224.1.2.3:12000"
			}
		}
	}
}
EOF

villas signal -l ${NUM_SAMPLES} -n random > input.dat

villas pipe -l ${NUM_SAMPLES} config.json node1 > output.dat < input.dat

villas compare input.dat output.dat
