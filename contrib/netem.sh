#!/bin/bash

IF=eth1

# Reset everything
tc qdisc del dev $IF root

# Root qdisc
tc qdisc add dev $IF root handle 4000 prio bands 3 priomap 0 0 0

# Netem qdsics
tc qdisc add dev $IF parent 4000:2 handle 4020 netem delay  500000
tc qdisc add dev $IF parent 4000:3 handle 4030 netem delay 1000000

# Filters
tc filter add dev $IF protocol ip u32 match ip dst 172.23.157.1 classid 4000:2
tc filter add dev $IF protocol ip u32 match ip dst 172.23.157.3 classid 4000:3

