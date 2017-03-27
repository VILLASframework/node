#!/bin/bash

LOCAL_CONF=${SRCDIR}/etc/loopback.conf
FETCHED_CONF=$(mktemp)

echo ${CONF}

# Start VILLASnode instance with local config (via advio)
villas-node file://${LOCAL_CONF} &

sleep 1

# Fetch config via API
curl -sX POST --data '{ "command" : "config" }' http://localhost/api/v1 2>/dev/null > ${FETCHED_CONF}

# Shutdown VILLASnode
kill $!

# Compare local config with the fetched one
diff -u <(cat ${LOCAL_CONF} | conf2json | jq -S .) <(cat ${FETCHED_CONF} | jq -S .config)

RC=$?

rm -f ${FETCHED_CONF}

exit $RC