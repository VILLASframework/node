#!/bin/bash
#
# Integration test for shift_ts hook.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

NUM_SAMPLES=${NUM_SAMPLES:-10}

OFFSET=-10.0
EPSILON=0.5

villas signal -l ${NUM_SAMPLES} -r 50 random | \
villas hook -o offset=${OFFSET} shift_ts | \
villas hook fix | \
villas hook -o format=json -o output=stats.json stats > /dev/null

if ! [ -f stats.json ]; then
    exit 1
fi

jq -e ".owd.mean + ${OFFSET} | length < ${EPSILON}" stats.json > /dev/null
