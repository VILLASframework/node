#!/usr/bin/env bash
#
# Integration test for node fmu.
#
# Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>
# SPDX-FileCopyrightText: 2014-2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

echo "Test not ready yet"
exit 99

set -e

DIR=$(mktemp -d)

cp ./asine.fmu ${DIR}
mkdir -p ${DIR}/fmu_asine

pushd ${DIR}

function finish {
    popd
    rm -rf ${DIR}
}
trap finish EXIT

cat > expect.dat <<EOF
1785455612.115810527+9.528000e-06(0)	1.57079632679489656
1785455612.115867093+3.417000e-06(1)	1.57079632679489656
1785455612.115900055+2.264000e-06(2)	1.57079632679489656
1785455612.115943016+1.099000e-05(3)	1.57079632679489656
1785455612.115990314+3.427000e-06(4)	1.57079632679489656
1785455612.116023817+1.273000e-06(5)	1.57079632679489656
1785455612.116046049+9.120000e-07(6)	1.57079632679489656
1785455612.116071286+1.012000e-06(7)	1.57079632679489656
1785455612.116093919+9.220000e-07(8)	1.57079632679489656
1785455612.116120008+9.820000e-07(9)	1.57079632679489656
EOF

cat > input.dat <<EOF
1785452573.098843010(0) 1.00000000000000000
1785452573.198961066(1) 1.00000000000000000
1785452573.298915255(2) 1.00000000000000000
1785452573.398927692(3) 1.00000000000000000
1785452573.498992077(4) 1.00000000000000000
1785452573.598886403(5) 1.00000000000000000
1785452573.698936040(6) 1.00000000000000000
1785452573.799023359(7) 1.00000000000000000
1785452573.898926701(8) 1.00000000000000000
1785452573.998933578(9) 1.00000000000000000
EOF

cat > config.json <<EOF
nodes = {
    file_output = {
        type = "file"
        uri = "output.dat"
        out = {

        }
    },
    signal_node = {
        type = "signal.v2"
        realtime = true

        limit = 10

        in = {
            signals = (
                { name = "in2",   signal = "constant", amplitude = 1  },
            )
        }
    },
    fmu_node = {
        type = "fmu"
        # Path to fmu file
        fmu_path = "${DIR}/asine.fmu"
        fmu_unpack_path = "${DIR}/fmu_asine"
        fmu_write_first = true
        stop_time = 10.0
        start_time = 0.0
        step_size = 0.2

        in = {
            signals = (
                { name = "In1", type = "float" }
            )
        }

        out = {
            signals = (
                { name = "Out1", type = "float" },
            )
        }
    }
}

paths = (
    {
        in = "signal_node",
        out = "fmu_node"
    },
    {
        in = "fmu_node",
        out = "file_output"
    },
)
EOF

villas node config.json &

sleep 3

kill %%
wait %%

villas compare -T expect.dat output.dat
