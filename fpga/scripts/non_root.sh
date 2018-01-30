#!/bin/bash

IOMMU_GROUP=24
PCI_BDF="0000:03:00.0"

modprobe vfio
modprobe vfio_pci

echo "10ee 7022" > /sys/bus/pci/drivers/vfio-pci/new_id
echo ${PCI_BDF} > /sys/bus/pci/drivers/vfio-pci/bind

groupadd -f fpga
usermod -G fpga -a svg

chgrp fpga /dev/vfio/${IOMMU_GROUP}
chmod g+rw /dev/vfio/${IOMMU_GROUP}


