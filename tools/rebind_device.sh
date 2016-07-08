#!/bin/sh

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
