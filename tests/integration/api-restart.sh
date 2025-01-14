#!/usr/bin/env bash
#
# Integration test for remote API
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


cat > base.json <<EOF
{
    "http": {
        "port": 8080
    }
}
EOF

cat > local.json <<EOF
{
    "http": {
        "port": 8080
    },
    "nodes": {
        "node1": {
             "type"   : "socket",
             "format": "csv",

             "in": {
             	"address"  : "*:12000"
             },
             "out": {
             	"address": "127.0.0.1:12001"
             }
        }
    },
    "paths": [
        {
             "in": "node1",
             "out": "node1",
             "hooks": [
             	{ "type": "print" }
             ]
        }
    ]
}
EOF

# Start with base configuration
villas node base.json &

# Wait for node to complete init
sleep 1

# Restart with configuration
curl -sX POST --data '{ "config": "local.json" }' http://localhost:8080/api/v2/restart

# Wait for node to complete init
sleep 2

# Fetch config via API
curl -s http://localhost:8080/api/v2/config > fetched.json

# Shutdown VILLASnode
kill %%
wait %%

# Compare local config with the fetched one
diff -u <(jq -S . < fetched.json) <(jq -S . < local.json)
