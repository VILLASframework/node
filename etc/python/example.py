#!/bin/env python3
''' Example Python config

 This example demonstrates how you can use Python to generate complex
 configuration files.

 To use this configuration, run the following commands:

    villas node <(python3 etc/python/example.py)

 Author: Steffen Vogel <post@steffenvogel.de>
 SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 SPDX-License-Identifier: Apache-2.0
'''

import json
import sys

N = 10

nodes = {
    'raspberry': {
        'type': 'socket',
        'layer': 'udp',
        'format': 'protobuf',

        'in': {
            'address': '*:12000',
        },
        'out': {
            'address': '1.2.3.4:12000'
        }
    }
}

for i in range(N):
    name = f'agent{i}'
    port = 12000 + i

    nodes[name] = {
        'type': 'socket',
        'layer': 'udp',
        'format': 'protobuf',

        'in': {
            'address': '*:12000',
            'signals': [
                {
                    'name': 'in',
                    'type': 'float'
                }
            ]
        },
        'out': {
            'address': f'5.6.7.8:{port}'
        }
    }

paths = [
    {
        'in': [f'agent{i}' for i in range(N)],
        'out': 'raspberry',
        'mode': 'any',
        'hooks': [
            {
                'type': 'print'
            }
        ]
    },
]

config = {
    'nodes': nodes,
    'paths': paths
}

json.dump(config, sys.stdout, indent=2)
