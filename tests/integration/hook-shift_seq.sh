#!/bin/bash
#
# Integration test for shift_seq hook.
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

OUTPUT_FILE=$(mktemp)

OFFSET=100

villas-signal random -l ${NUM_SAMPLES} -n | villas-hook shift_seq offset=${OFFSET} > ${OUTPUT_FILE}

# Compare shifted sequence no
diff -u \
	<(sed -re 's/^[0-9]+\.[0-9]+([\+\-][0-9]+\.[0-9]+(e[\+\-][0-9]+)?)?\(([0-9]+)\).*/\3/g' ${OUTPUT_FILE}) \
	<(seq ${OFFSET} $((${NUM_SAMPLES}+${OFFSET}-1)))

RC=$?

rm -f ${OUTPUT_FILE}

exit $RC
