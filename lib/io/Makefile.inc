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

LIB_FORMATS += json villas_binary villas_human csv raw
LIB_SRCS += lib/io/msg.c

# Enable Google Protobuf IO format
ifeq ($(WITH_FORMAT_PROTOBUF),1)
ifeq ($(shell $(PKGCONFIG) libprotobuf-c; echo $$?),0)
	LIB_SRCS += lib/io/protobuf.c lib/io/villas.pb-c.c
	LIB_PKGS += libprotobuf-c
	LIB_FORMATS += protobuf
endif
endif

%.pb-c.c %.pb-c.h: %.proto
	protoc-c --proto_path=$(SRCDIR) --c_out=$(SRCDIR) --dependency_out=$(BUILDDIR)/$*.d $(realpath $^)

$(BUILDDIR)/%.pb-c.o: LIB_CFLAGS += -I$(SRCDIR)
$(BUILDDIR)/lib/io/protobuf.o:  LIB_CFLAGS += -I$(SRCDIR)/lib/io
	
$(BUILDDIR)/lib/io/protobuf.o: lib/io/villas.pb-c.h
