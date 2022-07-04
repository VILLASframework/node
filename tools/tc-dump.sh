#!/bin/bash
#
# Dump Linux traffic control state to screen.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
##################################################################################

IF=$1

for cmd in qdisc filter class; do
	echo "======= $IF: $cmd ========"
	tc -s -d -p $cmd show dev $IF
done
