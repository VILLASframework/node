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

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})

CONFIG_FILE=$(mktemp)
FETCHED_CONF=$(mktemp)

cat > ${CONFIG_FILE} <<EOF
{
	"http" : {
		"port" : 8080
	},
	"nodes" : {
		"node1" : {
			"type" : "loopback"
		}
	}
}
EOF

ID=$(uuidgen)

# Start VILLASnode instance with local config (via advio)
villas-node ${CONFIG_FILE} &

# Wait for node to complete init
sleep 1

# Fetch config via API
curl -sX POST --data '{ "action" : "config", "id" : "'${ID}'" }' http://localhost:8080/api/v1 > ${FETCHED_CONF}

# Shutdown VILLASnode
kill $!

# Compare local config with the fetched one
diff -u <(jq -S .response < ${FETCHED_CONF}) <(jq -S . < ${CONFIG_FILE})
RC=$?

rm -f ${FETCHED_CONF} ${CONFIG_FILE}

exit $RC
