#!/bin/bash
#
# Integration test for scale hook.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASnode
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
##################################################################################

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

cat > script.lua <<EOF
global_var = 555
counter = 111

function prepare(cfg)
	assert(true)
	assert(cfg.custom.variable - 123.456 < 1e-9)
	assert(cfg.custom.list[0] == 1)
	assert(cfg.custom.list[1] - 2.0 < 1e-9)
	assert(cfg.custom.list[2] == true)
end

function start()
	counter = 0
end

function process(smp)
	assert(counter == smp.sequence)

	counter = counter + 1

	smp.data.signal1 = smp.data.signal2 + smp.data.signal3

	return 0
end
EOF

cat > config.json <<EOF
{
	"script": "script.lua",
	"custom": {
		"variable": 123.456,
		"list": [1, 2.0, true]
	},
	"signals": [
		{ "name": "global_var",     "expression": "global_var" },
		{ "name": "counter",        "expression": "counter" },
		{ "name": "signal1",        "expression": "smp.data.signal1" },
		{ "name": "ts_origin.sec",  "expression": "smp.ts_origin.sec" },
		{ "name": "ts_origin.sec",  "expression": "smp.ts_origin.nsec" },
		{ "name": "sequence",       "expression": "smp.sequence" }
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
# seconds.nanoseconds+offset(sequence)	global_var	counter	signal1	ts_origin.sec	ts_origin.sec	sequence
1551015508.801653200+6.430638e+07(0)	555.000000	1.000000	0.000000	1.000000	0.000000	0.000000
1551015508.901653200+6.430638e+07(1)	555.000000	2.000000	-0.400000	0.600000	0.100000	1.000000
1551015509.001653200+6.430638e+07(2)	555.000000	3.000000	-0.800000	0.200000	0.200000	2.000000
1551015509.101653200+6.430638e+07(3)	555.000000	4.000000	-1.200000	-0.200000	0.300000	3.000000
1551015509.201653200+6.430638e+07(4)	555.000000	5.000000	-1.600000	-0.600000	0.400000	4.000000
1551015509.301653200+6.430638e+07(5)	555.000000	6.000000	0.000000	-1.000000	0.500000	5.000000
1551015509.401653200+6.430638e+07(6)	555.000000	7.000000	0.400000	-0.600000	0.600000	6.000000
1551015509.501653200+6.430638e+07(7)	555.000000	8.000000	0.800000	-0.200000	0.700000	7.000000
1551015509.601653200+6.430638e+07(8)	555.000000	9.000000	1.200000	0.200000	0.800000	8.000000
1551015509.701653200+6.430638e+07(9)	555.000000	10.000000	1.600000	0.600000	0.900000	9.000000
EOF

villas hook lua -c config.json < input.dat > output.dat

villas compare output.dat expect.dat
