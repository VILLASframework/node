#!/bin/bash
#
# Integration test for shift_ts hook.
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

STATS_FILE=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-10}

OFFSET=-10.0
EPSILON=0.05

villas-signal random -l ${NUM_SAMPLES} -r 50 | \
villas-hook shift_ts -o offset=${OFFSET}     | \
villas-hook stats -o format=json -o output="${STATS_FILE}" > /dev/null

jq .owd ${STATS_FILE}

jq -e ".owd.mean + ${OFFSET} | length < ${EPSILON}" ${STATS_FILE} > /dev/null

RC=$?

rm ${STATS_FILE}

exit $RC
