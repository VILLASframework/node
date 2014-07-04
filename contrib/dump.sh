#!/bin/bash

IF=$1

for cmd in qdisc filter class; do
	echo "======= $if: $p ========"
	tc -s $cmd show dev $IF
done
