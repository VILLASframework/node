#!/bin/bash
#
# Integration loopback test for villas pipe.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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
