#!/bin/bash
#
# Integration loopback test for villas pipe.
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
