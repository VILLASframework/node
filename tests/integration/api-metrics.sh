#!/usr/bin/env bash
#
# Integration test for remote API
#
# Author: Steffen Vogel <steffen.vogel@opal-rt.com>
# SPDX-FileCopyrightText: 2025 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
    popd
    rm -rf ${DIR}
}
trap finish EXIT

cat > config.json <<EOF
{
    "http": {
        "port": 8080
    },
    "nodes": {
        "node1": {
            "type": "signal",

            "signal": "sine",
            "limit": 10,
            "values": 1,

            "in": {
                "hooks": [
                    {
                        "type": "stats",
                        "warmup": 0,
                        "buckets": 5
                    }
                ]
            }
        }
    },
    "paths": [
        {
            "in": "node1"
        }
    ]
}
EOF

cat <<EOF > expected
#TYPE HISTOGRAM rtp_jitter
rtp_jitter {node="node1" le="0"} 0
rtp_jitter {node="node1" le="0"} 0
rtp_jitter {node="node1" le="0"} 0
rtp_jitter {node="node1" le="0"} 0
rtp_jitter {node="node1" le="0"} 0
rtp_jitter {node="node1" le="+Inf"} 0
rtp_jitter_count  {node="node1"} 0

#TYPE HISTOGRAM rtp_pkts_lost
rtp_pkts_lost {node="node1" le="0"} 0
rtp_pkts_lost {node="node1" le="0"} 0
rtp_pkts_lost {node="node1" le="0"} 0
rtp_pkts_lost {node="node1" le="0"} 0
rtp_pkts_lost {node="node1" le="0"} 0
rtp_pkts_lost {node="node1" le="+Inf"} 0
rtp_pkts_lost_count  {node="node1"} 0

#TYPE HISTOGRAM rtp_loss_fraction
rtp_loss_fraction {node="node1" le="0"} 0
rtp_loss_fraction {node="node1" le="0"} 0
rtp_loss_fraction {node="node1" le="0"} 0
rtp_loss_fraction {node="node1" le="0"} 0
rtp_loss_fraction {node="node1" le="0"} 0
rtp_loss_fraction {node="node1" le="+Inf"} 0
rtp_loss_fraction_count  {node="node1"} 0

#TYPE HISTOGRAM signal_cnt
signal_cnt {node="node1" le="-8"} 0
signal_cnt {node="node1" le="-4"} 0
signal_cnt {node="node1" le="0"} 0
signal_cnt {node="node1" le="4"} 9
signal_cnt {node="node1" le="8"} 9
signal_cnt {node="node1" le="+Inf"} 10
signal_cnt_count  {node="node1"} 10

#TYPE HISTOGRAM age
age {node="node1" le="0"} 0
age {node="node1" le="0"} 0
age {node="node1" le="0"} 0
age {node="node1" le="0"} 0
age {node="node1" le="0"} 0
age {node="node1" le="+Inf"} 0
age_count  {node="node1"} 0

#TYPE HISTOGRAM owd
owd {node="node1" le="-8"} 0
owd {node="node1" le="-4"} 0
owd {node="node1" le="0"} 0
owd {node="node1" le="4"} 8
owd {node="node1" le="8"} 8
owd {node="node1" le="+Inf"} 9
owd_count  {node="node1"} 9

#TYPE HISTOGRAM gap_received
gap_received {node="node1" le="-8"} 0
gap_received {node="node1" le="-4"} 0
gap_received {node="node1" le="0"} 0
gap_received {node="node1" le="4"} 8
gap_received {node="node1" le="8"} 8
gap_received {node="node1" le="+Inf"} 9
gap_received_count  {node="node1"} 9

#TYPE HISTOGRAM gap_sent
gap_sent {node="node1" le="-8"} 0
gap_sent {node="node1" le="-4"} 0
gap_sent {node="node1" le="0"} 0
gap_sent {node="node1" le="4"} 8
gap_sent {node="node1" le="8"} 8
gap_sent {node="node1" le="+Inf"} 9
gap_sent_count  {node="node1"} 9

#TYPE HISTOGRAM reordered
reordered {node="node1" le="0"} 0
reordered {node="node1" le="0"} 0
reordered {node="node1" le="0"} 0
reordered {node="node1" le="0"} 0
reordered {node="node1" le="0"} 0
reordered {node="node1" le="+Inf"} 0
reordered_count  {node="node1"} 0

#TYPE HISTOGRAM skipped
skipped {node="node1" le="0"} 0
skipped {node="node1" le="0"} 0
skipped {node="node1" le="0"} 0
skipped {node="node1" le="0"} 0
skipped {node="node1" le="0"} 0
skipped {node="node1" le="+Inf"} 0
skipped_count  {node="node1"} 0

EOF

# Start VILLASnode instance with local config
villas node config.json &

# Wait for node to complete init
sleep 2

# Fetch config via API
curl -s http://localhost:8080/api/v2/metrics > metrics

# Shutdown VILLASnode
kill $!

# Check if metrics contain expected values
diff -u expected metrics
