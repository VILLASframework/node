#!/bin/bash

CONFIG_FILE=$(mktemp)
INPUT_FILE=$(mktemp)
OUTPUT_FILE=$(mktemp)

function prefix() {
	case $1 in
		node) P=$'\\\e[36mnode\\\e[39m: ' ;;
		pipe) P=$'\\\e[93mpipe\\\e[39m: ' ;;
		cmp)  P=$'\\\e[35mcmp\\\e[39m:  ' ;;
	esac
	
	sed -e "s/^/$P/"
}

cat > ${CONFIG_FILE} <<EOF
nodes = {
	node1 = {
		type = "socket";
		layer = "udp";

		local = "*:12000";
		remote = "127.0.0.1:12001"
	}
	
	node2 = {
		type = "socket";
		layer = "udp";

		local = "*:12001";
		remote = "127.0.0.1:12000"
	}
}

paths = (
	{
		in  = "node1",
		out = "node1"
	}
)
EOF

# Generate test data
villas-signal random -l 10 -n > ${INPUT_FILE}

# Start node
villas-node ${CONFIG_FILE} 2>&1 | prefix node &

# Wait for node to complete init
sleep 1

# Send / Receive data to node
(villas-pipe ${CONFIG_FILE} node2 > ${OUTPUT_FILE} < <(cat ${INPUT_FILE}; sleep 1; echo -n)) 2>&1 | prefix pipe

# Stop node
kill %1

# Compare data
villas-test-cmp ${INPUT_FILE} ${OUTPUT_FILE} 2>&1 | prefix cmp
RC=${PIPESTATUS[0]}

rm ${CONFIG_FILE} ${INPUT_FILE} ${OUTPUT_FILE}

exit $RC
