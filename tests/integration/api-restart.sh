#!/bin/bash
#
# Integration test for remote API
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

BASE_CONF=$(mktemp)
LOCAL_CONF=$(mktemp)
FETCHED_CONF=$(mktemp)

cat <<EOF > ${LOCAL_CONF}
{
	"http" : {
		"port" : 8080
	},
	"nodes" : {
		"node1" : {
			"type"   : "socket",
			"format" : "csv",
			
			"in" : {
				"address"  : "*:12000"
			},
			"out" : {
				"address" : "127.0.0.1:12001"
			}
		}
	},
	"paths" : [
		{
			"in" : "node1",
			"out" : "node1",
			"hooks" : [
				{ "type" : "print" }
			]
		}
	]
}
EOF

cat <<EOF > ${BASE_CONF}
{
	"http" : {
		"port" : 8080
	}
}
EOF

# Start with base configuration
villas-node ${BASE_CONF} &

# Wait for node to complete init
sleep 1

# Restart with configuration
curl -sX POST --data '{ "action" : "restart", "request" : { "config": "'${LOCAL_CONF}'" }, "id" : "5a786626-fbc6-4c04-98c2-48027e68c2fa" }' http://localhost:8080/api/v1

# Wait for node to complete init
sleep 2

# Fetch config via API
curl -sX POST --data '{ "action" : "config", "id" : "5a786626-fbc6-4c04-98c2-48027e68c2fa" }' http://localhost:8080/api/v1 > ${FETCHED_CONF}

# Shutdown VILLASnode
kill %%

# Compare local config with the fetched one
diff -u <(jq -S .response < ${FETCHED_CONF}) <(jq -S . < ${LOCAL_CONF})
RC=$?

rm -f ${LOCAL_CONF} ${FETCHED_CONF} ${BASE_CONF}

exit $RC
