#!/bin/bash

(curl 46.101.131.212:1026/v1/queryContext -s -S \
	--header 'Content-Type: application/json' \
	--header 'Accept: application/json' -d @- | python -mjson.tool) <<EOF
{
	"entities": [
		{
			"type": "type_one",
			"isPattern": "false",
			"id": "rtds_sub1"
		}
	]
}
EOF