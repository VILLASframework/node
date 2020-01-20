#!/bin/bash
#
# Integration test for limit_rate hook.
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

INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)
EXPECT_FILE=$(mktemp)

villas-signal -r 1000 -l 1000 -n sine > ${INPUT_FILE}
awk 'NR % 10 == 2' < ${INPUT_FILE} > ${EXPECT_FILE}

villas-hook -o rate=100 -o mode=origin limit_rate < ${INPUT_FILE} > ${OUTPUT_FILE}

# Compare only the data values
villas-test-cmp ${OUTPUT_FILE} ${EXPECT_FILE}

RC=$?

rm -f ${INPUT_FILE} ${OUTPUT_FILE} ${EXPECT_FILE}

exit $RC
