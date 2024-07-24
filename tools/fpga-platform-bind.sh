#!/bin/bash
#
# Bind ips to vfio
# Pass the device tree names e.g. "a0000000.dma a291229.switch ..." (space seperated) as comandline arguments.
#
# Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
# SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
# SPDX-License-Identifier: Apache-2.0

modprobe vfio_platform reset_required=0

i=1;
for device in "$@"
do
    echo "Device - $device";

    # Unbind Device from driver
    echo "$device > /sys/bus/platform/drivers/xilinx-vdma/unbind";

    # Bind device
    echo "vfio-platform > /sys/bus/platform/devices/$device/driver_override";
    echo "$device > /sys/bus/platform/drivers/vfio-platform/bind";

    i=$((i + 1));
done
