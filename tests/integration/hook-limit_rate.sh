#!/bin/bash
#
# Integration test for limit_rate hook.
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

villas signal -r 1000 -l 1000 -n sine > input.dat

awk 'NR % 10 == 2' < input.dat > expect.dat

villas hook -o rate=100 -o mode=origin limit_rate < input.dat > output.dat

villas compare output.dat expect.dat
