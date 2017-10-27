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

LIB_SRCS += $(addprefix lib/io/,json.c villas_binary.c villas_human.c csv.c raw.c msg.c)

WITH_PROTOBUF ?= 1

# Enable Google Protobuf IO format
ifeq ($(WITH_PROTOBUF),1)
ifeq ($(shell $(PKGCONFIG) libprotobuf-c; echo $$?),0)
	LIB_SRCS += lib/io/protobuf.c lib/io/villas.pb-c.c
	LIB_PKGS += libprotobuf-c
endif
endif

%.pb-c.h: %.proto
	protoc-c --proto_path=$(SRCDIR) --c_out=$(SRCDIR) $(realpath $^)

lib/io/villas.pb-c.c \
lib/io/protobuf.c: | lib/io/villas.pb-c.h

