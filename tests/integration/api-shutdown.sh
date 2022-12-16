#!/bin/bash
#
# Integration test for remote API
#
# @author Steffen Vogel <post@steffenvogel.de>
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
	}
}
EOF

# Start without a configuration
timeout -s SIGKILL 3 \
villas node config.json & 

# Wait for node to complete init
sleep 1

# Restart with configuration
curl -sX POST http://localhost:8080/api/v2/shutdown

# Wait returns the return code of villas node
# which will be 0 (success) in case of normal shutdown
# or <>0 (fail) in case the 3 second timeout was reached
wait $!
