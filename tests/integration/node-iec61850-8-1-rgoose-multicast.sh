#!/usr/bin/env bash
#
# Integration test for Multicast Routed GOOSE messages.
#
# Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
# SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set -e

# Check if user is superuser.
if [[ "${EUID}" -ne 0 ]]; then
    echo "Please run as root"
    exit 99
fi

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
    popd
    rm -rf ${DIR}
}
trap finish EXIT

NUM_SAMPLES=${NUM_SAMPLES:-10}

cat > config.json << EOF
{
  "nodes": {
    "infile": {
        "type": "file",
        "uri": "input.dat",
        "signals": [{ "name": "counter" }]
    },

    "outfile": {
        "type": "file",
        "uri": "output.dat"
    },

    "goose": {
      "type": "iec61850-8-1",

      "keys": [
        {
          "id": 1,
          "security": "aes_128_gcm",
          "signature": "none",
          "string": "0123456789ABCDEF"
        }
      ],

      "out": {
        "routed": true,
        "publishers": [
          {
            "go_cb_ref": "simpleIOGenericIO/LLN0$GO$gcbAnalogValues",
            "data_set_ref": "simpleIOGenericIO/LLN0$AnalogValues",
            "conf_rev": 1,
            "time_allowed_to_live": 500,
            "app_id": 16385,
            "data": [
              {
                "mms_type": "int32",
                "signal": "counter"
              }
            ]
          }
        ],
        "signals": [
          {
            "name": "counter",
            "type": "integer"
          }
        ]
      },

      "in": {
        "routed": true,
        "subscribers": {
          "sub": {
            "go_cb_ref": "simpleIOGenericIO/LLN0$GO$gcbAnalogValues",
            "app_id": 1000,
            "trigger": "change"
          }
        },
        "signals": [
          {
            "name": "counter",
            "type": "integer",
            "mms_type": "int32",
            "subscriber": "sub",
            "index": 0
          }
        ]
      }
    }
  },

  "paths": [
    {
      "in": "infile",
      "hooks": [{
          "type": "cast",
          "new_type": "integer",
          "signals": ["counter"]
      }],
      "out": "goose"
    },
    {
      "in": "goose",
      "hooks": [{
          "type": "cast",
          "new_type": "float",
          "signals": ["counter"]
      }],
      "out": "outfile"
    }
  ]
}
EOF

villas signal -l ${NUM_SAMPLES} -n counter -r 10 > input.dat

VILLAS_LOG_PREFIX="[node] " \
villas node config.json &

# Wait for node to complete
sleep $((NUM_SAMPLES / 10 + 2))

kill %%
wait %%

# Send / Receive data to node
VILLAS_LOG_PREFIX="[compare] " \
villas compare -T input.dat output.dat
