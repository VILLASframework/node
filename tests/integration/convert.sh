#!/bin/bash
#
# Integration test for villas-convert tool
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

OUTPUT_FILE=$(mktemp)
INPUT_FILE=$(mktemp)

FORMATS="villas.human csv json"

for FORMAT in ${FORMATS}; do
    villas-signal -v5 -n -l20 mixed | \
    tee ${INPUT_FILE} | \
    villas-convert -o ${FORMAT} | \
    villas-convert -i ${FORMAT} | \
    tee ${OUTPUT_FILE}
    
    villas-test-cmp ${INPUT_FILE} ${OUTPUT_FILE}
done
