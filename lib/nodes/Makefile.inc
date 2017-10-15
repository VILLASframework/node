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

ifeq ($(PLATFORM),Linux)
	WITH_FPGA     ?= 1
	WITH_CBUILDER ?= 1
	WITH_LOOPBACK ?= 1
endif

WITH_TEST_RTT  ?= 1
WITH_FILE      ?= 1
WITH_SIGNAL    ?= 1
WITH_NGSI      ?= 1
WITH_WEBSOCKET ?= 1
WITH_SOCKET    ?= 1
WITH_ZEROMQ    ?= 1
WITH_NANOMSG   ?= 1
WITH_SHMEM     ?= 1
WITH_STATS     ?= 1
WITH_INFLUXDB  ?= 1

# Enabled loopback node-type
ifeq ($(WITH_LOOPBACK),1)
	LIB_SRCS += lib/nodes/loopback.c 
	LIB_CFLAGS += -DWITH_LOOPBACK
endif

# Enabled Cbuilder node-type
ifeq ($(WITH_CBUILDER),1)
	LIB_SRCS += lib/nodes/cbuilder.c 
	LIB_CFLAGS += -DWITH_CBUILDER
endif

# Enable InfluxDB node-type
ifeq ($(WITH_INFLUXDB),1)
	LIB_SRCS += lib/nodes/influxdb.c
	LIB_CFLAGS += -DWITH_INFLUXDB
endif

# Enable stats node-type
ifeq ($(WITH_STATS),1)
	LIB_SRCS += lib/nodes/stats.c
	LIB_CFLAGS += -DWITH_STATS
endif

# Enable file node-type
ifeq ($(WITH_FILE),1)
	LIB_SRCS += lib/nodes/file.c
	LIB_CFLAGS += -DWITH_FILE
endif

# Enable shared memory node-type
ifeq ($(WITH_SHMEM),1)
	LIB_SRCS += lib/nodes/shmem.c
	LIB_CFLAGS += -DWITH_SHMEM
endif

# Enable signal generator node-type
ifeq ($(WITH_SIGNAL),1)
	LIB_SRCS += lib/nodes/signal.c
	LIB_CFLAGS += -DWITH_SIGNAL
endif

# Enable RTT test node-tyoe
ifeq ($(WITH_TEST_RTT),1)
	LIB_SRCS += lib/nodes/test_rtt.c
	LIB_CFLAGS += -DWITH_TEST_RTT
endif

# Enable VILLASfpga support when libxil is available
ifeq ($(WITH_FPGA),1)
ifeq ($(shell $(PKGCONFIG) libxil; echo $$?),0)
	LIB_SRCS    += $(addprefix lib/nodes/, fpga.c) \
	               $(addprefix lib/kernel/, pci.c vfio.c) \

	-include lib/fpga/Makefile.inc

	LIB_PKGS    += libxil
	LIB_CFLAGS += -DWITH_FPGA
endif
endif

# Enable OPAL-RT Asynchronous Process support (will result in 32bit binary!!!)
ifeq ($(WITH_OPAL),1)
ifneq ($(wildcard thirdparty/libopal/include/opal/AsyncApi.h),)
	LIB_OBJS    += opal.o

	LIB_CFLAGS  += -I thirdparty/libopal/include/opal/ -DWITH_OPAL
	LIB_LDFLAGS += -L/lib/i386-linux-gnu/ -L/usr/lib/i386-linux-gnu/ -Lthirdparty/libopal/
	LIB_LDLIBS  += -lOpalAsyncApiCore -lOpalCore -lOpalUtils -lirc

	# libOpalAsyncApi is a 32bit library. So we need to build everything in 32bit
	CFLAGS      += -m32
	LDFLAGS     += -m32
endif
endif

# Enable Socket node type when libnl3 is available
ifeq ($(WITH_SOCKET),1)
	LIB_SRCS    += lib/nodes/socket.c
	LIB_CFLAGS  += -DWITH_SOCKET

	# libnl3 is optional but required for network emulation and IRQ pinning
	ifeq ($(shell $(PKGCONFIG) libnl-route-3.0; echo $$?),0)
		LIB_SRCS    += $(addprefix lib/kernel/, nl.c tc.c if.c)
		LIB_PKGS    += libnl-route-3.0
	endif
endif

# Enable nanomsg node type when libnanomsg is available
ifeq ($(WITH_NANOMSG),1)
ifeq ($(shell $(PKGCONFIG) nanomsg; echo $$?),0)
	LIB_SRCS    += lib/nodes/nanomsg.c
	LIB_PKGS    += nanomsg
	LIB_CFLAGS  += -DWITH_NANOMSG
else ifeq ($(shell $(PKGCONFIG) libnanomsg; echo $$?),0)
	LIB_SRCS    += lib/nodes/nanomsg.c
	LIB_PKGS    += libnanomsg
	LIB_CFLAGS  += -DWITH_NANOMSG
endif
endif

# Enable ZeroMQ node type when libzmq is available
ifeq ($(WITH_ZEROMQ),1)
ifeq ($(shell $(PKGCONFIG) libzmq; echo $$?),0)
	LIB_SRCS    += $(addprefix lib/nodes/, zeromq.c)
	LIB_PKGS    += libzmq
	LIB_CFLAGS  += -DWITH_ZEROMQ
endif
endif

# Enable NGSI support
ifeq ($(WITH_NGSI),1)
ifeq ($(shell $(PKGCONFIG) libcurl jansson; echo $$?),0)
	LIB_SRCS    += lib/nodes/ngsi.c
	LIB_PKGS    += libcurl jansson
	LIB_CFLAGS  += -DWITH_NGSI
endif
endif

# Enable WebSocket support
ifeq ($(WITH_WEBSOCKET),1)
ifeq ($(shell $(PKGCONFIG) libwebsockets jansson; echo $$?),0)
	LIB_SRCS    += lib/nodes/websocket.c
	LIB_PKGS    += libwebsockets jansson
	LIB_CFLAGS  += -DWITH_WEBSOCKET
endif
endif
