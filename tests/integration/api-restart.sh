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

# Start with base configuration
villas node &

# Wait for node to complete init
sleep 1

# Restart with configuration
curl -sX POST --data '{ "config": "'${LOCAL_CONF}'" }' http://localhost:8080/api/v2/restart
echo

# Wait for node to complete init
sleep 2

# Fetch config via API
curl -s http://localhost:8080/api/v2/config > ${FETCHED_CONF}

# Shutdown VILLASnode
kill %%

# Compare local config with the fetched one
diff -u <(jq -S . < ${FETCHED_CONF}) <(jq -S . < ${LOCAL_CONF})
RC=$?

rm -f ${LOCAL_CONF} ${FETCHED_CONF} ${BASE_CONF}

exit ${RC}
