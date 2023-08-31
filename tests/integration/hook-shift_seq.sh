#!/bin/bash
#
# Integration test for shift_seq hook.
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

OFFSET=100

villas signal -l ${NUM_SAMPLES} -n random > input.dat

villas hook -o offset=${OFFSET} shift_seq > output.dat < input.dat

# Compare shifted sequence no
diff -u \
	<(sed -re '/^#/d;s/^[0-9]+\.[0-9]+([\+\-][0-9]+\.[0-9]+(e[\+\-][0-9]+)?)?\(([0-9]+)\).*/\3/g' output.dat) \
	<(seq ${OFFSET} $((${NUM_SAMPLES}+${OFFSET}-1)))
