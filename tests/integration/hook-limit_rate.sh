#!/usr/bin/env bash
#
# Integration test for limit_rate hook.
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

villas signal -r 1000 -l 1000 -n sine > input.dat

awk 'NR % 10 == 2' < input.dat > expect.dat

villas hook -o rate=100 -o mode=origin limit_rate < input.dat > output.dat

villas compare output.dat expect.dat
