#!/bin/bash

(curl http://46.101.131.212:1026/v1/updateContext -s -S \
	--header 'Content-Type: application/json' \
	--header 'Accept: application/json' -d @- | python -mjson.tool) <<EOF
{
    "contextElements": [
        {
            "type": "type_one",
            "isPattern": "false",
            "id": "rtds_sub3",
            "attributes": [
                {
                    "name": "v1",
                    "type": "float",
                    "value": "26.5"
                },
                {
                    "name": "v2",
                    "type": "float",
                    "value": "763"
                }
            ]
        }
    ],
    "updateAction": "UPDATE"
} 
EOF