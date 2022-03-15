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
	},
	"nodes": {
		"testnode1": {
			"type": "websocket",
			"dummy": "value1"
		},
		"testnode2": {
			"type": "socket",
			"dummy": "value2",

			"in": {
				"address": "*:12001",
				"signals": [
					{ "name": "sig1", "unit": "Volts",  "type": "float", "init": 123.0 },
					{ "name": "sig2", "unit": "Ampere", "type": "integer", "init": 123 }
				]
			},
			"out": {
				"address": "127.0.0.1:12000"
			}
		}
	},
	"paths": [
		{
			"in": "testnode2",
			"out": "testnode1"
		}
	]
}
EOF

# Start VILLASnode instance with local config
villas node config.json &

# Wait for node to complete init
sleep 1

# Fetch config via API
curl -s http://localhost:8080/api/v2/paths > fetched.json

# Shutdown VILLASnode
kill $!

# Compare local config with the fetched one
jq -e '(.[0].in | index("testnode2")) and (.[0].out | index("testnode1")) and (. | length == 1)' fetched.json > /dev/null
