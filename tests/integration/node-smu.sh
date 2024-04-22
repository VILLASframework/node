#!/usr/bin/env bash
#
# Integration loopback test using villas node for smu node-type.
#
# Author: Alexandra Bach <alexandra.bach@eonerc.rwth-aachen.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

echo "Test not yet supported"
exit 99

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
    popd
    rm -rf ${DIR}
}
trap finish EXIT

mkdir ./logs

cat > config.json <<EOF
{
    "nodes": {
        "smu-node": {
            "type": "smu",
            "signals": [
                { name = "ch1" type = "float"},
                { name = "ch2" type = "float"},
                { name = "signal3" type = "float"},
                { name = "signal4" type = "float"},
                { name = "signal5" type = "float"},
                { name = "signal6" type = "float"},
                { name = "signal7" type = "float"}
            ]
            "vectorize": 1000,
            "sample_rate": "FS_10kSPS"
        }
    },
    "paths": [
        {
             "in": "smu-node"
             "out": "test"
        }
    ]
}
EOF

villas node config.json
