#!/bin/bash

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
ID=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 16 | head -n 1)
ENDPOINT=${ENDPOINT:-http://localhost:80/api/v1}

echo "Issuing API request: action=${ACTION}, id=${ID}, request=${REQUEST}, endpoint=${ENDPOINT}"

curl -s -X POST --data "{
    \"action\" : \"${ACTION}\",
    \"id\": \"${ID}\",
    \"request\": ${REQUEST}
}" ${ENDPOINT} | jq .
