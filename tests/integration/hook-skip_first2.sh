#!/bin/bash

OUTPUT_FILE=$(mktemp)

SKIP=50

villas-signal random -r 1 -l ${NUM_SAMPLES} -n | villas-hook skip_first samples=${SKIP} > ${OUTPUT_FILE}

LINES=$(wc -l < ${OUTPUT_FILE})

rm ${OUTPUT_FILE}

# Test condition
(( ${LINES} == ${NUM_SAMPLES} - ${SKIP} ))