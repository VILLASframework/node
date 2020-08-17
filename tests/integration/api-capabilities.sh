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

FETCHED_CONF=$(mktemp)

# Start without a configuration
villas-node &
PID=$!

# Wait for node to complete init
sleep 1

# Fetch capabilities
curl -s http://localhost:8080/api/v2/capabilities > ${FETCHED_CONF}

kill $PID
wait

jq -e '.apis | index( "capabilities" ) != null' < ${FETCHED_CONF}
RC=$?

rm ${FETCHED_CONF}

exit $RC
