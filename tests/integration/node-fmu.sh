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
pushd ${DIR}

function finish {
    popd
    rm -rf ${DIR}
}
trap finish EXIT

cat > expect.dat <<EOF
1773060970.283043274+4.316100e-05(0)	0.00000000000000000
1773060970.292971482+4.407200e-05(1)	1.57079632679489656
1773060970.302955555+4.345100e-05(2)	1.57079632679489656
1773060970.312947222+4.071700e-05(3)	1.57079632679489656
1773060970.322965369+3.425400e-05(4)	1.57079632679489656
1773060970.332968828+3.238100e-05(5)	1.57079632679489656
1773060970.342982707+3.703000e-05(6)	1.57079632679489656
1773060970.353000303+5.085500e-05(7)	1.57079632679489656
1773060970.362930736+3.564700e-05(8)	1.57079632679489656
1773060970.373010167+4.531500e-05(9)	1.57079632679489656
1773060970.382907157+3.850200e-05(10)	1.57079632679489656
EOF

cat > config.json <<EOF
nodes = {
    file_output = {
        type = "file"
        # format = "csv"
        uri = "/workspaces/node/fmu_temp/output.dat"
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
        fmu_path = "/workspaces/node/fmu_temp/asine.fmu"
        fmu_unpack_path = "/workspaces/node/fmu_temp/fmu_asine"
        fmu_writefirst = true
        stopTime = 10.0
        startTime = 0.0
        stepSize = 0.2

        in = {
            signals = (
                {name = "In1", type = "float"}
            )
        }

        out = {
            signals = (
                {name = "Out1", type = "float"},
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

sleep 2

kill %%
wait %%

villas compare expect.dat output.dat
