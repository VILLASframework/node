#!/bin/bash
#
# Integration test for remote API
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
##################################################################################

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
	},
	"nodes": {
		"node1": {
			"type": "loopback"
		}
	}
}
EOF

ID=$(uuidgen)

# Start VILLASnode instance with local config
villas node config.json &

# Wait for node to complete init
sleep 1

# Fetch config via API
curl -s http://localhost:8080/api/v2/config > fetched.json

# Shutdown VILLASnode
kill $!

# Compare local config with the fetched one
diff -u <(jq -S . < fetched.json) <(jq -S . < config.json)
