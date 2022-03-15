#!/bin/bash
#
# Integration test for remote API
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT


cat > config.json <<EOF
{
	"http": {
		"port": 8080
	}
}
EOF

# Start without a configuration
timeout -s SIGKILL 3 \
villas node config.json & 

# Wait for node to complete init
sleep 1

# Restart with configuration
curl -sX POST http://localhost:8080/api/v2/shutdown

# Wait returns the return code of villas node
# which will be 0 (success) in case of normal shutdown
# or <>0 (fail) in case the 3 second timeout was reached
wait $!
