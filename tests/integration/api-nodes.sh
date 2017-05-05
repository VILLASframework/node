#!/bin/bash
#
# Integration test for remote API
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

CONFIG_FILE=$(mktemp)
FETCHED_NODES=$(mktemp)

cat > ${CONFIG_FILE} <<EOF
nodes = {
	testnode1 = {
		type = "websocket";
		dummy = "value1";
	},
	testnode2 = {
		type = "socket";
		dummy = "value2";

		local = "*:12001";
		remote = "localhost:12000";
	}
}
EOF

# Start VILLASnode instance with local config (via advio)
villas-node ${CONFIG_FILE} &

# Wait for node to complete init
sleep 1

# Fetch config via API
curl -sX POST --data '{ "action" : "nodes", "id" : "5a786626-fbc6-4c04-98c2-48027e68c2fa" }' http://localhost/api/v1 > ${FETCHED_NODES}

# Shutdown VILLASnode
kill $!

# Compare local config with the fetched one
jq -e '.response[0].name == "testnode1" and .response[0].type == "websocket" and .response[0].id == 0 and (.response | length == 2)' ${FETCHED_NODES} > /dev/null
RC=$?

rm -f ${CONFIG_FILE} ${FETCHED_NODES}

exit $RC