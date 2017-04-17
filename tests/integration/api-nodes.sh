#!/bin/bash

set -e

CONFIG_FILE=$(mktemp)
FETCHED_NODES=$(mktemp)

cat > ${CONFIG_FILE} <<EOF
nodes = {
	testnode1 = {
		type = "websocket";
		dummy = "value1";
	},
	testnode2 = {
		type = "socket";
		dummy = "value2";
		
		local = "*:12001";
		remote = "localhost:12000";
	}
}
EOF

# Start VILLASnode instance with local config (via advio)
villas-node ${CONFIG_FILE} &

# Wait for node to complete init
sleep 1

# Fetch config via API
curl -sX POST --data '{ "action" : "nodes", "id" : "5a786626-fbc6-4c04-98c2-48027e68c2fa" }' http://localhost/api/v1 > ${FETCHED_NODES}

# Shutdown VILLASnode
kill $!

# Compare local config with the fetched one
jq -e '.response[0].name == "testnode1" and .response[0].type == "websocket" and .response[0].id == 0 and (.response | length == 2)' ${FETCHED_NODES} > /dev/null
RC=$?

rm -f ${CONFIG_FILE} ${FETCHED_NODES}

exit $RC