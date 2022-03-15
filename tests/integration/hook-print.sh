#!/bin/bash
#
# Integration test for print hook.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

NUM_SAMPLES=${NUM_SAMPLES:-100}

villas signal -v 1 -r 10 -l ${NUM_SAMPLES} -n random > input.dat

villas hook -o format=villas.human -o output=output1.dat print > output2.dat < input.dat

if [ -s output1.dat -a -s output2.dat ]; then
    villas compare output1.dat output2.dat input.dat
else
    exit -1
fi
