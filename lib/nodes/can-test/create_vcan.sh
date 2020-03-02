#!/bin/bash

# source: https://en.wikipedia.org/wiki/SocketCAN

ip link add dev vcan0 type vcan
ip link set up vcan0
ip link show vcan0
