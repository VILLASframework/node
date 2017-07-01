#!/bin/bash
#
# Integration loopback test using villas-node.
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

CONFIG_FILE=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

function prefix() {
	case $1 in
		node) P=$'\\\e[36mnode\\\e[39m: ' ;;
		pipe) P=$'\\\e[93mpipe\\\e[39m: ' ;;
		cmp)  P=$'\\\e[35mcmp\\\e[39m:  ' ;;
	esac

	sed -e "s/^/$P/"
}
NUM_SAMPLES=${NUM_SAMPLES:-10}

cat > ${CONFIG_FILE} <<EOF
nodes = {
	node1 = {
		type = "socket";
		layer = "udp";

		local = "*:12000";
		remote = "127.0.0.1:12001"
	}

	node2 = {
		type = "socket";
		layer = "udp";

		local = "*:12001";
		remote = "127.0.0.1:12000"
	}
}

paths = (
	{
		in  = "node1",
		out = "node1"
	}
)
EOF

# Generate test data
villas-signal random -l ${NUM_SAMPLES} -n > ${INPUT_FILE}

# Start node
villas-node ${CONFIG_FILE} 2>&1 | prefix node &

# Wait for node to complete init
sleep 1

# Send / Receive data to node
(villas-pipe ${CONFIG_FILE} node2 > ${OUTPUT_FILE} < <(cat ${INPUT_FILE}; sleep 1; echo -n)) 2>&1 | prefix pipe

# Stop node
kill %1

# Compare data
villas-test-cmp ${INPUT_FILE} ${OUTPUT_FILE} 2>&1 | prefix cmp
RC=${PIPESTATUS[0]}

rm ${CONFIG_FILE} ${INPUT_FILE} ${OUTPUT_FILE}

exit $RC
