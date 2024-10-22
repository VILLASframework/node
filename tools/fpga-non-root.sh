#!/usr/bin/env bash
#
# Setup VFIO for non-root users
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

# PCI-e parameters of FPGA card
PCI_BDF="0000:03:00.0"
PCI_VID="10ee"
PCI_PID="7022"

modprobe vfio
modprobe vfio_pci
modprobe vfio_iommu_type1

IOMMU_GROUP=`basename $(readlink /sys/bus/pci/devices/${PCI_BDF}/iommu_group)`

# bind to vfio driver
echo "${PCI_VID} ${PCI_PID}" > /sys/bus/pci/drivers/vfio-pci/new_id
echo "${PCI_BDF}" > /sys/bus/pci/drivers/vfio-pci/bind

groupadd -f fpga
usermod -G fpga -a svg
usermod -G fpga -a dkr

chgrp fpga /dev/vfio/${IOMMU_GROUP}
chmod g+rw /dev/vfio/${IOMMU_GROUP}
