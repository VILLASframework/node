#!/bin/bash
#
# Integration test for dp hook.
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

# Test is not ready yet
exit 99


INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)
RECON_FILE=$(mktemp)

NUM_SAMPLES=10000
RATE=5000
F0=50

OPTS="-o f0=${F0} -o rate=${RATE} -o signal=0 -o harmonics=0,1,3,5,7"

villas-signal sine -v1 -l ${NUM_SAMPLES} -f ${F0} -r ${RATE} -n > ${INPUT_FILE}

villas-hook dp -o inverse=false ${OPTS} < ${INPUT_FILE} > ${OUTPUT_FILE}

villas-hook dp -o inverse=true ${OPTS} < ${OUTPUT_FILE} > ${RECON_FILE}

exit 0

# Compare only the data values
villas-test-cmp ${OUTPUT_FILE} ${EXPECT_FILE}
RC=$?

rm -f ${INPUT_FILE} ${OUTPUT_FILE} ${EXPECT_FILE}

exit ${RC}
