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

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})

LOCAL_CONF=${SCRIPTPATH}/../../etc/loopback.json

FETCHED_CONF=$(mktemp)
FETCHED_JSON_CONF=$(mktemp)
LOCAL_JSON_CONF=$(mktemp)

ID=$(uuidgen)

# Start VILLASnode instance with local config (via advio)
villas-node file://${LOCAL_CONF} &

# Wait for node to complete init
sleep 1

# Fetch config via API
curl -sX POST --data '{ "action" : "config", "id" : "'${ID}'" }' http://localhost/api/v1 > ${FETCHED_CONF}

# Shutdown VILLASnode
kill $!

# Compare local config with the fetched one
diff -u <(jq -S .response < ${FETCHED_CONF}) <(jq -S . < ${LOCAL_CONF})
RC=$?

rm -f ${FETCHED_CONF} ${FETCHED_JSON_CONF} ${LOCAL_JSON_CONF}

exit $RC