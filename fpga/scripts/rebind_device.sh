#!/bin/bash
#
# Detach and rebind a PCI device to a PCI kernel driver
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASnode
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
##################################################################################

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
