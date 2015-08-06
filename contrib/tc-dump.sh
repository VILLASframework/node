#!/bin/bash

IF=$1

for cmd in qdisc filter class; do
	echo "======= $IF: $cmd ========"
	tc -s -d -p $cmd show dev $IF
done
