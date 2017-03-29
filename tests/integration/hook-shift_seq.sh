#!/bin/bash

OUTPUT_FILE=$(mktemp)

OFFSET=100

villas-signal random -l ${NUM_SAMPLES} -n | villas-hook shift_seq offset=${OFFSET} > ${OUTPUT_FILE}

# Compare shifted sequence no
diff -u \
	<(sed -re 's/^[0-9]+\.[0-9]+([\+\-][0-9]+\.[0-9]+(e[\+\-][0-9]+)?)?\(([0-9]+)\).*/\3/g' ${OUTPUT_FILE}) \
	<(seq ${OFFSET} $((${NUM_SAMPLES}+${OFFSET}-1)))

RC=$?

rm -f ${OUTPUT_FILE}

exit $RC
