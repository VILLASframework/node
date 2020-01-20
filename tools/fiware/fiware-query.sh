#!/bin/bash
#
# Query FIWARE Orion Context Broker
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

(curl http://46.101.131.212:1026/v1/queryContext -s -S \
	--header 'Content-Type: application/json' \
	--header 'Accept: application/json' -d @- | python -mjson.tool) <<EOF
{
	"entities": [
		{
			"type": "type_one",
			"isPattern": "false",
			"id": "rtds_sub3"
		},
		{
			"type": "type_two",
			"isPattern": "false",
			"id": "rtds_sub4"
		}
	]
}
EOF
