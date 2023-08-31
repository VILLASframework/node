#!/bin/bash
#
# Integration test for cast hook.
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
# seconds.nanoseconds+offset(sequence)	signal0	signal1	signal2	signal3	signal4
1551015508.801653200(0)	0.022245	0.000000	-1.000000	1.000000	0.000000
1551015508.901653200(1)	0.015339	58.778500	-1.000000	0.600000	0.100000
1551015509.001653200(2)	0.027500	95.105700	-1.000000	0.200000	0.200000
1551015509.101653200(3)	0.040320	95.105700	-1.000000	-0.200000	0.300000
1551015509.201653200(4)	0.026079	58.778500	-1.000000	-0.600000	0.400000
1551015509.301653200(5)	0.049262	0.000000	1.000000	-1.000000	0.500000
1551015509.401653200(6)	0.014883	-58.778500	1.000000	-0.600000	0.600000
1551015509.501653200(7)	0.023232	-95.105700	1.000000	-0.200000	0.700000
1551015509.601653200(8)	0.015231	-95.105700	1.000000	0.200000	0.800000
1551015509.701653200(9)	0.060849	-58.778500	1.000000	0.600000	0.900000
EOF

cat > expect.dat <<EOF
# seconds.nanoseconds+offset(sequence)	signal0	test[V]	signal2	signal3	signal4
1551015508.801653200(0)	0.022245	0	-1.000000	1.000000	0.000000
1551015508.901653200(1)	0.015339	58	-1.000000	0.600000	0.100000
1551015509.001653200(2)	0.027500	95	-1.000000	0.200000	0.200000
1551015509.101653200(3)	0.040320	95	-1.000000	-0.200000	0.300000
1551015509.201653200(4)	0.026079	58	-1.000000	-0.600000	0.400000
1551015509.301653200(5)	0.049262	0	1.000000	-1.000000	0.500000
1551015509.401653200(6)	0.014883	-58	1.000000	-0.600000	0.600000
1551015509.501653200(7)	0.023232	-95	1.000000	-0.200000	0.700000
1551015509.601653200(8)	0.015231	-95	1.000000	0.200000	0.800000
1551015509.701653200(9)	0.060849	-58	1.000000	0.600000	0.900000
EOF

villas hook cast -o new_name=test -o new_unit=V -o new_type=integer -o signal=signal1 < input.dat > output.dat

villas compare output.dat expect.dat
