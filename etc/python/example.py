#!/bin/env python3
''' Example Python config

 This example demonstrates how you can use Python to generate complex
 configuration files.

 To use this configuration, run the following commands:

    villas node <(python3 etc/python/example.py)

 @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 @license GNU General Public License (version 3)

 VILLASnode

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
