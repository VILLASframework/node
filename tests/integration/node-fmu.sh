#!/usr/bin/env bash
#
# Integration test for node fmu.
#
# Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>
# SPDX-FileCopyrightText: 2014-2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set -e

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
DIR=$(mktemp -d)

cp $SCRIPTPATH/Dahlquist.fmu ${DIR}
mkdir -p ${DIR}/fmu_dahl

pushd ${DIR}

function finish {
    popd
    rm -rf ${DIR}
}
trap finish EXIT

cat > expect.dat <<EOF
1786460106.299091281+8.066000e-06(0)	0.34867844009999999
1786460106.299275927+8.210000e-07(1)	0.12157665459056928
1786460106.299307566+4.410000e-07(2)	0.04239115827521620
1786460106.299332903+4.610000e-07(3)	0.01478088294143459
1786460106.299357048+3.310000e-07(4)	0.00515377520732011
1786460106.299382386+3.600000e-07(5)	0.00179701029991443
1786460106.299407693+3.210000e-07(6)	0.00062657874821780
1786460106.299434673+3.410000e-07(7)	0.00021847450052839
1786460106.299460101+3.510000e-07(8)	0.00007617734804587
1786460106.299485739+2.900000e-07(9)	0.00002656139888759
EOF

cat > config.json <<EOF
nodes = {
    file_output = {
        type = "file"
        uri = "output.dat"
        out = {

        }
    },
    fmu_node = {
        type = "fmu"
        # Path to fmu file
        fmu_path = "${DIR}/Dahlquist.fmu"
        fmu_unpack_path = "${DIR}/fmu_dahl"
        fmu_write_first = true
        stop_time = 10.0
        start_time = 0.0
        step_size = 1.0

        in = {
            signals = ()
        }

        out = {
            signals = (
                { name = "x", type = "float" },
            )
        }
    }
}

paths = (
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

villas compare -T expect.dat output.dat
