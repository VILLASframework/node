#!/bin/bash
#
# Reset PCI devices like FPGAs after a reflash
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

if [ "$#" -ne 1 ]; then
        echo "usage: $0 BUS:DEV:FNC"
        exit 1
fi

BDF=$1

echo "1" > /sys/bus/pci/devices/$BDF/remove
echo "1" > /sys/bus/pci/rescan
echo "1" > /sys/bus/pci/devices/$BDF/enable

