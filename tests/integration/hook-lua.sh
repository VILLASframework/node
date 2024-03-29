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

cat > config.json <<EOF
{
    "signals": [
        { "name": "signal1_positive", "expression": "smp.data.signal1 >= 0", "type": "boolean" },
        { "name": "abs(signal1)",     "expression": "math.abs(smp.data.signal1)" },
        { "name": "signal4_scaled",   "expression": "smp.data.signal4 * 100 + 55" },
        { "name": "sequence",         "expression": "smp.sequence", "type": "integer" },
        { "name": "ts_origin",        "expression": "smp.ts_origin[0] + smp.ts_origin[1] * 1e-9" }
    ]
}
EOF

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
# seconds.nanoseconds+offset(sequence)	signal1_positive	abs(signal1)	signal4_scaled	sequence	ts_origin
1551015508.801653200+6.430676e+07(0)	1	0.000000	55.000000	0	1551015508.801653
1551015508.901653200+6.430676e+07(1)	1	0.587785	65.000000	1	1551015508.901653
1551015509.001653200+6.430676e+07(2)	1	0.951057	75.000000	2	1551015509.001653
1551015509.101653200+6.430676e+07(3)	1	0.951057	85.000000	3	1551015509.101653
1551015509.201653200+6.430676e+07(4)	1	0.587785	95.000000	4	1551015509.201653
1551015509.301653200+6.430676e+07(5)	1	0.000000	105.000000	5	1551015509.301653
1551015509.401653200+6.430676e+07(6)	0	0.587785	115.000000	6	1551015509.401653
1551015509.501653200+6.430676e+07(7)	0	0.951057	125.000000	7	1551015509.501653
1551015509.601653200+6.430676e+07(8)	0	0.951057	135.000000	8	1551015509.601653
1551015509.701653200+6.430676e+07(9)	0	0.587785	145.000000	9	1551015509.701653
EOF

villas hook lua -c config.json < input.dat > output.dat

villas compare output.dat expect.dat
