#!/usr/bin/env bash
#
# Integration OpenDSS test using VILLASnode.
#
# Author: Jitpanu Maneeratpongsuk <jitpanu.maneeratpongsuk@rwth-aachen.de>
# SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
    popd
    rm -rf ${DIR}
}
trap finish EXIT

cat > load.dat <<EOF
1741796341.163774487(0)	3655.8894
1741796341.263774487(1)	3378.2656
1741796341.363774487(2)	3287.0156
1741796341.463774487(3)	3149.8606
1741796341.563774487(4)	3133.5386
1741796341.663774487(5)	3253.4573
1741796341.763774487(6)	3547.7632
1741796341.863774487(7)	4037.5439
1741796341.963774487(8)	4492.7231
1741796342.063774487(9)	4751.9297
EOF

cat > expect.dat <<EOF
1741796341.163774487+2.584580e+06(1)	1.00000000000000000	0.00000000000000000	3693.36181640625000000	2050.57128906250000000
1741796341.263774487+2.584580e+06(2)	2.00000000000000000	0.00000000000000000	3438.36425781250000000	1890.97180175781250000
1741796341.363774487+2.584579e+06(3)	3.00000000000000000	0.00000000000000000	3353.65258789062500000	1838.60375976562500000
1741796341.463774487+2.584579e+06(4)	4.00000000000000000	0.00000000000000000	3217.87255859375000000	1755.30749511718750000
1741796341.563774487+2.584579e+06(5)	5.00000000000000000	0.00000000000000000	3200.91357421875000000	1744.95617675781250000
1741796341.663774487+2.584579e+06(6)	6.00000000000000000	0.00000000000000000	3322.39086914062500000	1819.35546875000000000
1741796341.763774487+2.584579e+06(7)	7.00000000000000000	0.00000000000000000	3594.51586914062500000	1988.35876464843750000
1741796341.863774487+2.584579e+06(8)	8.00000000000000000	0.00000000000000000	4037.45263671875000000	2270.64013671875000000
1741796341.963774487+2.584579e+06(9)	9.00000000000000000	0.00000000000000000	4438.25732421875000000	2533.86572265625000000
1741796342.063774487+2.584579e+06(10)	10.00000000000000000	0.00000000000000000	4662.09277343750000000	2683.89404296875000000
EOF

cat > Test.DSS << EOF
Clear

Set DefaultBaseFrequency=60

new object=circuit.Test basekV=12.47 phases=3

new transformer.t1 xhl=6
~ wdg=1 bus=n2 conn=wye kV=12.47 kVA=6000 %r=0.5
~ wdg=2 bus=n3 conn=wye kV=4.16  kVA=6000 %r=0.5

new line.line1 bus1=sourcebus bus2=n2 length=1 units=km
new line.line2 bus1=n3 bus2=n4 length=1 units=km

new load.load1 bus1=n4 phases=3 kV=4.16 kW=5400 pf=0.9 model=1

new monitor.t1_p element=Transformer.t1 terminal=1 ppolar=no mode=65

set voltagebases=[12.47, 4.16]
calcvoltagebases

set mode=daily
set number=1
EOF

cat > config.json << EOF
{
  "nodes": {
      "node1": {
           "type": "opendss",
           "file_path" : "Test.DSS",
           "in": {
             "list": [
                {"name": "load1", "type": "load", "data": ["kW"]}
             ]
           },
           "out": {
             "list": ["t1_p"]
           }
      },
      "node2": {
            "type": "file",
            "uri": "load.dat",
            "in": {
              "epoch_mode": "original",
              "epoch": 10,
              "rate": 4,
              "buffer": 0
            }
      }
  },
  "paths": [
      {
        "in": "node2",
        "out": "node1"
      },
      {
        "in": "node1",
        "hooks": [
            {"type": "print",
            "output": "output.dat"}
        ]
      }
  ]
}
EOF

VILLAS_LOG_PREFIX="[node] " \
villas node config.json &

# Wait for node to complete init
sleep 2

kill %%
wait %%

# Send / Receive data to node
VILLAS_LOG_PREFIX="[compare] " \
villas compare expect.dat output.dat
