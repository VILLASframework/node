#!/bin/bash

set -e

# Configuration

HOST=$(hostname -s)

#LOCAL_IP=10.10.12.2
LOCAL_IP=127.0.0.1
LOCAL_PORT=12001

#REMOTE_IP=10.10.12.3
REMOTE_IP=127.0.0.1
REMOTE_PORT=12002

#LOCAL_DIR=/villas/server/
#REMOTE_DIR=/villas/server/
LOCAL_DIR=/home/stv0g/workspace/rwth/acs/villas/server
REMOTE_DIR=$LOCAL_DIR
######################### End of Configuration ################################
# There's no need to change something below this line

SSH="ssh -T $REMOTE_IP"

TEST_CONFIG="
	affinity = 0x02;	# Mask of cores the server should run on
	priority = 50;		# Scheduler priority for the server
	debug = 10;		# The level of verbosity for debug messages
	stats = 3;		# The interval in seconds fo path statistics

	nodes = {
		remote = {
			type = \"udp\";
			local = \"$LOCAL_IP:$LOCAL_PORT\";
			remote = \"$REMOTE_IP:$REMOTE_PORT\";
		}
	};
"


REMOTE_CONFIG="
	affinity = 0x02;	# Mask of cores the server should run on
	priority = 50;		# Scheduler priority for the server
	debug = 0;		# The level of verbosity for debug messages
	stats = 0;		# The interval in seconds fo path statistics

	nodes = {
		remote = {
			type = \"udp\";
			local = \"$REMOTE_IP:$REMOTE_PORT\";
			remote = \"$LOCAL_IP:$LOCAL_PORT\";
		},
	};

	paths = ({
		in = \"remote\";
		out = \"remote\";
	});
"

# Start remote server
$SSH -n "echo '$REMOTE_CONFIG' | sudo $REMOTE_DIR/server -" &
REMOTE_PID=$!
echo "Started remote server with pid: $REMOTE_PID"

# Start tests
echo "$TEST_CONFIG" | $LOCAL_DIR/test - rtt -f 3 -c 100000  3>> test_output

# Stop remote server
$SSH sudo kill $REMOTE_PID
echo "Stopped remote server with pid: $REMOTE_PID"