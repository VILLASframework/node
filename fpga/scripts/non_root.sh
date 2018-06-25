#!/bin/bash
#
# Setup VFIO for non-root users
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
