#!/bin/sh

if [ "$#" -ne 1 ]; then
        echo "usage: $0 BUS:DEV:FNC"
        exit 1
fi

BDF=$1

echo "1" > /sys/bus/pci/devices/$BDF/remove
echo "1" > /sys/bus/pci/rescan
echo "1" > /sys/bus/pci/devices/$BDF/enable

