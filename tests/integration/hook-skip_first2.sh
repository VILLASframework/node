#!/bin/bash
#
# Integration test for skip_first hook.
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

OUTPUT_FILE=$(mktemp)

SKIP=50

villas-signal -r 1 -l ${NUM_SAMPLES} -n random | \
villas-hook -o samples=${SKIP} skip_first > ${OUTPUT_FILE}

LINES=$(sed -re '/^#/d' ${OUTPUT_FILE} | wc -l)

rm ${OUTPUT_FILE}

# Test condition
(( ${LINES} == ${NUM_SAMPLES} - ${SKIP} ))
