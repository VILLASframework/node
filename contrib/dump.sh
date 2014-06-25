#!/bin/bash

for if in lo eth1; do
	for p in qdisc filter class; do
		echo "======= $if: $p ========"
		tc -s $p show dev $if
	done
	echo
done
