#!/bin/bash

CONFIG_FILE=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

cat > ${CONFIG_FILE} <<- EOF
nodes = {
	node1 = {
		type = "socket";
		layer = "udp";
		
		local = "*:12000";
		remote = "127.0.0.1:12000"
	}
}
EOF

# Generate test data
villas-signal random -l 10 -n > ${INPUT_FILE}

# We delay EOF of the INPUT_FILE by 1 second in order to wait for incoming data to be received
villas-pipe ${CONFIG_FILE} node1 > ${OUTPUT_FILE} < <(cat ${INPUT_FILE}; sleep 1; echo -n)

# Comapre data
villas-test-cmp ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

rm ${OUTPUT_FILE} ${INPUT_FILE} ${CONFIG_FILE}

exit $RC