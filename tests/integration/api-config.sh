#!/bin/bash
#
# Integration test for remote API
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
	},
	"nodes": {
		"node1": {
			"type": "loopback"
		}
	}
}
EOF

ID=$(uuidgen)

# Start VILLASnode instance with local config
villas node config.json &

# Wait for node to complete init
sleep 1

# Fetch config via API
curl -s http://localhost:8080/api/v2/config > fetched.json

# Shutdown VILLASnode
kill $!

# Compare local config with the fetched one
diff -u <(jq -S . < fetched.json) <(jq -S . < config.json)
