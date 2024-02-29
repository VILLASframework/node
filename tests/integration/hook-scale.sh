#!/usr/bin/env bash
#
# Integration test for scale hook.
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

cat > input.dat <<EOF
# seconds.nanoseconds(sequence)	random	sine	square	triangle	ramp
1551015508.801653200(0)	0.022245	0.000000	-1.000000	1.000000	0.000000
1551015508.901653200(1)	0.015339	0.587785	-1.000000	0.600000	0.100000
1551015509.001653200(2)	0.027500	0.951057	-1.000000	0.200000	0.200000
1551015509.101653200(3)	0.040320	0.951057	-1.000000	-0.200000	0.300000
1551015509.201653200(4)	0.026079	0.587785	-1.000000	-0.600000	0.400000
1551015509.301653200(5)	0.049262	0.000000	1.000000	-1.000000	0.500000
1551015509.401653200(6)	0.014883	-0.587785	1.000000	-0.600000	0.600000
1551015509.501653200(7)	0.023232	-0.951057	1.000000	-0.200000	0.700000
1551015509.601653200(8)	0.015231	-0.951057	1.000000	0.200000	0.800000
1551015509.701653200(9)	0.060849	-0.587785	1.000000	0.600000	0.900000
EOF

cat > expect.dat <<EOF
# seconds.nanoseconds+offset(sequence)	signal0	signal1	signal2	signal3	signal4
1551015508.801653200(0)	0.022245	0.000000	-1.000000	1.000000	55.000000
1551015508.901653200(1)	0.015339	0.587785	-1.000000	0.600000	65.000000
1551015509.001653200(2)	0.027500	0.951057	-1.000000	0.200000	75.000000
1551015509.101653200(3)	0.040320	0.951057	-1.000000	-0.200000	85.000000
1551015509.201653200(4)	0.026079	0.587785	-1.000000	-0.600000	95.000000
1551015509.301653200(5)	0.049262	0.000000	1.000000	-1.000000	105.000000
1551015509.401653200(6)	0.014883	-0.587785	1.000000	-0.600000	115.000000
1551015509.501653200(7)	0.023232	-0.951057	1.000000	-0.200000	125.000000
1551015509.601653200(8)	0.015231	-0.951057	1.000000	0.200000	135.000000
1551015509.701653200(9)	0.060849	-0.587785	1.000000	0.600000	145.000000
EOF

villas hook scale -o scale=100 -o offset=55 -o signal=signal4 < input.dat > output.dat

villas compare output.dat expect.dat
