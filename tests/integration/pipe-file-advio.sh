#!/bin/bash

CONFIG_FILE=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

URI=https://1Nrd46fZX8HbggT:badpass@rwth-aachen.sciebo.de/public.php/webdav/node/tests/pipe

cat > ${CONFIG_FILE} <<EOF
nodes = {
	remote_file_out = {
		type = "file",
		
		out = {
			uri = "${URI}"
			mode = "w+"
		},
	},
	remote_file_in = {
		type = "file",
		
		in = {
			uri = "${URI}"
			mode = "r"
			epoch_mode = "original"
		}
	}
}
EOF

villas signal sine -n -l 10 > ${INPUT_FILE}

villas pipe -s ${CONFIG_FILE} remote_file_out < ${INPUT_FILE}

villas pipe -r ${CONFIG_FILE} remote_file_in > ${OUTPUT_FILE}

villas test-cmp -j ${INPUT_FILE} ${OUTPUT_FILE}
RC=$?

rm -f ${CONFIG_FILE} ${INPUT_FILE} ${OUTPUT_FILE}

exit $RC