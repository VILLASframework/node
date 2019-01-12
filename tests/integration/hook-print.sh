#!/bin/bash
#
# Integration test for print hook.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

OUTPUT_FILE1=$(mktemp)
OUTPUT_FILE2=$(mktemp)
INPUT_FILE=$(mktemp)

NUM_SAMPLES=${NUM_SAMPLES:-100}

# Prepare some test data
villas-signal -v 1 -r 10 -l ${NUM_SAMPLES} -n random > ${INPUT_FILE}

villas-hook -o format=villas.human -o output=${OUTPUT_FILE1} print > ${OUTPUT_FILE2} < ${INPUT_FILE}

# Compare only the data values
villas-test-cmp ${OUTPUT_FILE1} ${OUTPUT_FILE2} ${INPUT_FILE}
RC=$?

rm -f ${OUTPUT_FILE1} ${OUTPUT_FILE2} ${INPUT_FILE}

exit $RC
