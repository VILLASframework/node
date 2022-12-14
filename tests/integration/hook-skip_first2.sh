#!/bin/bash
#
# Integration test for skip_first hook.
#
# @author Steffen Vogel <post@steffenvogel.de>
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

SKIP=50

villas signal -r 1 -l ${NUM_SAMPLES} -n random > input.dat

villas hook -o samples=${SKIP} skip_first > output.dat < input.dat

LINES=$(sed -re '/^#/d' output.dat | wc -l)

# Test condition
(( ${LINES} == ${NUM_SAMPLES} - ${SKIP} ))
