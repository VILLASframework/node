#!/usr/bin/env bash
#
# Detach and rebind a PCI device to a PCI kernel driver
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

if [ "$#" -ne 2 ]; then
    echo "usage: $0 BUS:DEV:FNC DRIVER"
    exit 1
fi

BDF=$1
DRIVER=$2

VENDOR=$(cut -b3- /sys/bus/pci/devices/${BDF}/vendor)
DEVICE=$(cut -b3- /sys/bus/pci/devices/${BDF}/device)

SYSFS_DEVICE=/sys/bus/pci/devices/${BDF}
SYSFS_DRIVER=/sys/bus/pci/drivers/${DRIVER}

echo "Device: $VENDOR $DEVICE $BDF"

if [ -L "${SYSFS_DEVICE}/driver" ] && [ -d "${SYSFS_DEVICE}/driver" ]; then
    echo ${BDF}              > ${SYSFS_DEVICE}/driver/unbind
fi

echo "${VENDOR} ${DEVICE}" > ${SYSFS_DRIVER}/new_id
echo "${BDF}"              > ${SYSFS_DRIVER}/bind
echo "${VENDOR} ${DEVICE}" > ${SYSFS_DRIVER}/remove_id
