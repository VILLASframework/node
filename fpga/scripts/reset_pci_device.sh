#!/bin/bash
#
# Reset PCI devices like FPGAs after a reflash
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2017-2022, Institute for Automation of Complex Power Systems, EONERC
# SPDX-License-Identifier: Apache-2.0
##################################################################################

if [ "$#" -ne 1 ]; then
        echo "usage: $0 BUS:DEV.FNC"
        exit 1
fi

BDF=$1

echo "1" > /sys/bus/pci/devices/${BDF}/remove
echo "1" > /sys/bus/pci/rescan
echo "1" > /sys/bus/pci/devices/${BDF}/reset
