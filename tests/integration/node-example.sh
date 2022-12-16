#!/bin/bash
#
# Test for the example node-type
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
##################################################################################

set -e

echo "The example node does nothing useful which we could test"
exit 99

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

cat > expect.dat <<EOF
1553690684.041211800-4.313770e-02(5)	6.303265	0.492616	0.309017	-1.000000	0.800000	0.050000
1553690684.051211800-5.287260e-02(6)	5.673902	0.148827	0.368125	-1.000000	0.760000	0.060000
1553690684.061211800-6.266780e-02(7)	5.896198	0.232320	0.425779	-1.000000	0.720000	0.070000
1553690684.071211800-7.256350e-02(8)	5.788125	0.152309	0.481754	-1.000000	0.680000	0.080000
1553690684.081211800-8.251890e-02(9)	6.748635	0.608491	0.535827	-1.000000	0.640000	0.090000
EOF

cat > config.json <<EOF
{
	"nodes": {
		"example_node1": {
			"type": "example",

			"setting1": 66
		},
		"file": {
			"type": "file",
			"uri": "output.dat"
		}
	},
	"paths": [
		{
			"in": "example_node1",
			"out": "file",
		}
	]
}
EOF

villas node config.json

villas compare output.dat expect.dat
