#!/bin/bash
#
# Integration loopback test for villas pipe.
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
##################################################################################

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

NUM_SAMPLES=${NUM_SAMPLES:-100}

cat > config.json << EOF
{
	"nodes": {
		"node1": {
			"type": "iec61850-9-2",
		
			"interface": "lo",
		
			"out": {
				"svid": "1234",
				"signals": [
					{ "iec_type": "float32" },
					{ "iec_type": "float32" },
					{ "iec_type": "float32" },
					{ "iec_type": "float32" }
				]
			},
			"in": {
				"signals": [
					{ "iec_type": "float32" },
					{ "iec_type": "float32" },
					{ "iec_type": "float32" },
					{ "iec_type": "float32" }
				]
			}
		}
	}
}
EOF

villas signal -l ${NUM_SAMPLES} -v 4 -n random > input.dat

villas pipe -l ${NUM_SAMPLES} config.json node1 > output.dat < input.dat

villas compare -T input.dat output.dat
