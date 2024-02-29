#!/usr/bin/env bash
#
# Dump Linux traffic control state to screen.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

IF=$1

for cmd in qdisc filter class; do
	echo "======= $IF: $cmd ========"
	tc -s -d -p $cmd show dev $IF
done
