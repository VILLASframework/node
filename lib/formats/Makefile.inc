# Makefile.
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
###################################################################################

LIB_SRCS += $(addprefix lib/formats/,json.c villas.c csv.c)

WITH_HDF5 ?= 0

ifeq ($(PLATFORM),Darwin)
	HDF5_PREFIX ?= /opt/local
else
	HDF5_PREFIX ?= /usr
endif

ifeq ($(WITH_HDF5),1)
ifneq ($(wildcard $(HDF5_PREFIX)/include/hdf5_hl.h),)
	LIB_SRCS += lib/formats/hdf5.c
endif
endif
