#!/bin/bash
#
# Integration loopback test for villas pipe.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
##################################################################################

set -e

if [[ "${EUID}" -ne 0 ]]; then
	echo "Test requires root permissions"
	exit 99
fi

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

NUM_SAMPLES=${NUM_SAMPLES:-100}
NUM_VALUES=${NUM_VALUES:-4}
FORMAT=${FORMAT:-villas.binary}
VECTORIZES="1 10"

for LAYER in udp ip eth unix; do
for VECTORIZE in ${VECTORIZES}; do

case ${LAYER} in
	udp)
		LOCAL="127.0.0.1:12000"
		REMOTE="127.0.0.1:12000"
		;;

	ip)
		# We use IP protocol number 253 which is reserved for experimentation and testing according to RFC 3692
		LOCAL="127.0.0.1:254"
		REMOTE="127.0.0.1:254"
		;;

	eth)
		LOCAL="00:00:00:00:00:00%lo:34997"
		REMOTE="00:00:00:00:00:00%lo:34997"
		;;
		
	unix)
		LOCAL=$(mktemp)
		REMOTE=${LOCAL}
		;;
esac

cat > config.json << EOF
{
	"nodes": {
		"node1": {
			"type": "socket",
			
			"vectorize": ${VECTORIZE},
			"format": "${FORMAT}",
			"layer": "${LAYER}",

			"out": {
				"address": "${REMOTE}"
			},
			"in": {
				"address": "${LOCAL}",
				"signals": {
					"count": ${NUM_VALUES},
					"type": "float"
				}
			}
		}
	}
}
EOF

villas signal -v ${NUM_VALUES} -l ${NUM_SAMPLES} -n random > input.dat

villas pipe -l ${NUM_SAMPLES} config.json node1 < input.dat > output.dat

villas compare ${CMPFLAGS} input.dat output.dat

done; done
