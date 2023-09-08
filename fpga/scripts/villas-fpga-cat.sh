#!/bin/bash
# 
# Author: Niklas Eiling <niklas.eiling@rwth-aachen.de>
# SPDX-FileCopyrightText: 2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

CWD=$(dirname -- "$0")

${CWD}/villas-fpga-ctrl -c ${CWD}/../../etc/fpgas.json --connect "2<->stdout"
