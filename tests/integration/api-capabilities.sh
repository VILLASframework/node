#!/bin/bash
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

cat > config.json <<EOF
{
	"http": {
		"port": 8080
	}
}
EOF

# Start without a configuration
villas node config.json &

# Wait for node to complete init
sleep 1

# Fetch capabilities
curl -s http://localhost:8080/api/v2/capabilities > fetched.json

kill %%
wait %%

jq -e '.apis | index( "capabilities" ) != null' < fetched.json
