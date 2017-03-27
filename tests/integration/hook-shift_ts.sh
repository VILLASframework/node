#!/bin/bash

STATS_FILE=$(mktemp)

OFFSET=10
EPSILON=0.05

villas-signal sine -l 10 -r 10 | villas-hook shift_ts offset=${OFFSET} | villas-hook stats format=\"json\" output=\"${STATS_FILE}\" > /dev/null

#jq .owd ${STATS_FILE}

jq -e ".owd.mean - ${OFFSET} | length < ${EPSILON}" ${STATS_FILE} > /dev/null

RC=$?

rm ${STATS_FILE}

exit $RC