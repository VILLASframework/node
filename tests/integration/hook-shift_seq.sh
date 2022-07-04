#!/bin/bash
#
# Integration test for shift_seq hook.
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

OFFSET=100

villas signal -l ${NUM_SAMPLES} -n random > input.dat

villas hook -o offset=${OFFSET} shift_seq > output.dat < input.dat

# Compare shifted sequence no
diff -u \
	<(sed -re '/^#/d;s/^[0-9]+\.[0-9]+([\+\-][0-9]+\.[0-9]+(e[\+\-][0-9]+)?)?\(([0-9]+)\).*/\3/g' output.dat) \
	<(seq ${OFFSET} $((${NUM_SAMPLES}+${OFFSET}-1)))
