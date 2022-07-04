#!/bin/bash
#
# Integration test for shift_ts hook.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
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
