#!/bin/bash
#
# Stress test for remote API
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

SCRIPT=$(realpath $0)
SCRIPTPATH=$(dirname ${SCRIPT})

LOCAL_CONF=${SCRIPTPATH}/../../etc/loopback.json

# Start VILLASnode instance with local config (via advio)
villas-node file://${LOCAL_CONF} &
PID=$!

# Wait for node to complete init
sleep 1

RUNS=100
FAILED=0
SUCCESS=0

FIFO=$(mktemp -t fifo.XXXXXX)
rm ${FIFO}
mkfifo ${FIFO}

# Fifo must be opened in both directions!
# https://www.gnu.org/software/libc/manual/html_node/FIFO-Special-Files.html#FIFO-Special-Files
# Quote: "However, it has to be open at both ends simultaneously before you can proceed to
#            do any input or output operations on it"
exec 5<>${FIFO}

JOBS=""
for J in $(seq 1 ${RUNS}); do
	(
		set -e
		trap "echo error-trap >> ${FIFO}" ERR

		FETCHED_CONF=$(mktemp)
		
		curl -sX POST --data '{ "action" : "config", "id" : "'$(uuidgen)'" }' http://localhost:8080/api/v1 > ${FETCHED_CONF}
		diff -u <(jq -S .response < ${FETCHED_CONF}) <(jq -S . < ${LOCAL_CONF})
		RC=$?
		
		if [ "$RC" -eq "0" ]; then
			echo success >&5
		else
			echo error >&5
		fi
	) &
	JOBS+=" $!"
done

echo "Waiting for jobs to complete: $JOBS"
wait $JOBS
kill $PID
wait $PID

echo "Check return codes"
FAILED=0
SUCCESS=0
for J in $(seq 1 ${RUNS}); do
	read status <&5
	
	if [ "$status" == "success" ]; then
		let SUCCESS++
	else
		let FAILED++
	fi
done

echo "Success: ${SUCCESS} / ${RUNS}"
echo "Failed:  ${FAILED} / ${RUNS}"

if [ "$FAILED" -gt "0" ]; then
	exit 1;
fi
