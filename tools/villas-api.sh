#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

# Check if all required commands exist

if ! command -v curl &> /dev/null; then
    echo "curl could not be found"
    exit 1
fi

if ! command -v jq &> /dev/null; then
    echo "jq could not be found"
    exit 1
fi

if [ $# -lt 1 ]; then
    echo "usage: villas-api ACTION [REQUEST-JSON]"
    exit 1
fi

ACTION=$1
REQUEST=${2:-\{\}}
ENDPOINT=${ENDPOINT:-http://localhost:8080/api/v2}

# GET actions have no body; actions carrying a request body use POST
case "${ACTION}" in
    status|capabilities|config|nodes|paths)
        METHOD=GET
        ;;
    *)
        METHOD=POST
        ;;
esac

echo "Issuing API request: ${METHOD} ${ENDPOINT}/${ACTION}, request=${REQUEST}"

if [ "${METHOD}" = "GET" ]; then
    curl -s "${ENDPOINT}/${ACTION}" | jq .
else
    curl -s -X POST \
        -H "Content-Type: application/json" \
        --data "${REQUEST}" \
        "${ENDPOINT}/${ACTION}" | jq .
fi
