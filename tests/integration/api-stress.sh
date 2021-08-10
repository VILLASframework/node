#!/bin/bash
#
# Stress test for remote API
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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
set -x

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

LOCAL_CONF=${SRCDIR}/etc/loopback.json

# Start VILLASnode instance with local config
villas node ${LOCAL_CONF} &

# Wait for node to complete init
sleep 1

RUNS=100
FAILED=0
SUCCESS=0

FIFO=$(mktemp -p ${DIR} -t fifo.XXXXXX -u)
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

		FETCHED_CONF=$(mktemp -p ${DIR})
		
		curl -s http://localhost:8080/api/v2/config > ${FETCHED_CONF}
		diff -u <(jq -S . < ${FETCHED_CONF}) <(jq -S . < ${LOCAL_CONF})
		RC=$?
		
		if [ "$RC" -eq "0" ]; then
			echo success >&5
		else
			echo error >&5
		fi
	) &
	JOBS+=" $!"
done

echo "Waiting for jobs to complete: ${JOBS}"
wait ${JOBS}

kill %1
wait %1

echo "Check return codes"
for J in $(seq 1 ${RUNS}); do
	read -t 10 -u 5 STATUS
	
	if [ "${STATUS}" == "success" ]; then
		let ++SUCCESS
	else
		let ++FAILED
	fi
done

echo "Success: ${SUCCESS} / ${RUNS}"
echo "Failed:  ${FAILED} / ${RUNS}"

(( ${FAILED} == 0 ))
