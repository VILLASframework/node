#!/bin/bash
#
# Integration test for remote API
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

cat > ${CONFIG_FILE} <<EOF
{
	"http" : {
		"port" : 8080
	}
}
EOF

# Start without a configuration
timeout -s SIGKILL 3 villas-node ${CONFIG_FILE} & 

# Wait for node to complete init
sleep 1

# Restart with configuration
curl -sX POST --data '{ "action" : "shutdown", "id" : "5a786626-fbc6-4c04-98c2-48027e68c2fa" }' http://localhost:8080/api/v1

rm ${CONFIG_FILE}

# Wait returns the return code of villas-node
# which will be 0 (success) in case of normal shutdown
# or <>0 (fail) in case the 3 second timeout was reached
wait $!
