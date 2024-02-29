#!/usr/bin/env bash
#
# Integration test for print hook.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

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
