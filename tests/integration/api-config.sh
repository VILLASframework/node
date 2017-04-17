#!/bin/bash

set -e

LOCAL_CONF=${SRCDIR}/etc/loopback.conf

FETCHED_CONF=$(mktemp)
FETCHED_JSON_CONF=$(mktemp)
LOCAL_JSON_CONF=$(mktemp)

# Start VILLASnode instance with local config (via advio)
villas-node file://${LOCAL_CONF} &

# Wait for node to complete init
sleep 1

# Fetch config via API
curl -sX POST --data '{ "action" : "config", "id" : "5a786626-fbc6-4c04-98c2-48027e68c2fa" }' http://localhost/api/v1 > ${FETCHED_CONF}

# Shutdown VILLASnode
kill $!

conf2json < ${LOCAL_CONF} | jq -S . > ${LOCAL_JSON_CONF}
jq -S .response < ${FETCHED_CONF} > ${FETCHED_JSON_CONF}

# Compare local config with the fetched one
diff -u ${FETCHED_JSON_CONF} ${LOCAL_JSON_CONF}
RC=$?

rm -f ${FETCHED_CONF} ${FETCHED_JSON_CONF} ${LOCAL_JSON_CONF}

exit $RC