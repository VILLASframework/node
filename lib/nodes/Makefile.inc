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

# Enabled loopback node-type
ifeq ($(WITH_NODE_LOOPBACK),1)
	LIB_NODES   += loopback
endif

# Enabled Cbuilder node-type
ifeq ($(WITH_NODE_CBUILDER),1)
	LIB_NODES   += cbuilder
endif

# Enable InfluxDB node-type
ifeq ($(WITH_NODE_INFLUXDB),1)
	LIB_NODES   += influxdb
endif

# Enable stats node-type
ifeq ($(WITH_NODE_STATS),1)
	LIB_NODES   += stats
endif

# Enable file node-type
ifeq ($(WITH_NODE_FILE),1)
	LIB_NODES   += file
	WITH_IO      = 1
endif

# Enable shared memory node-type
ifeq ($(WITH_NODE_SHMEM),1)
	LIB_NODES   += shmem
endif

# Enable signal generator node-type
ifeq ($(WITH_NODE_SIGNAL),1)
	LIB_NODES   += signal
endif

# Enable RTT test node-tyoe
ifeq ($(WITH_NODE_TEST_RTT),1)
	LIB_NODES   += test_rtt
	WITH_IO      = 1
endif

# Enable IEC61850 node-types when libiec61850 is available
ifeq ($(WITH_NODE_IEC61850),1)
ifeq ($(shell $(PKGCONFIG) --atleast-version=1.2.0 libiec61850; echo $$?),0)
	LIB_SRCS    += lib/nodes/iec61850_sv.c
	LIB_NODES   += iec61850
	LIB_PKGS    += libiec61850
endif
endif

# Enable VILLASfpga support when libvillas-fpga is available
ifeq ($(WITH_NODE_FPGA),1)
ifeq ($(shell $(PKGCONFIG) libvillas-fpga; echo $$?),0)
	LIB_PKGS    += libvillas-fpga
	LIB_NODES   += fpga
endif
endif

# Enable OPAL-RT Asynchronous Process support (will result in 32bit binary!!!)
ifeq ($(WITH_NODE_OPAL),1)
ifneq ($(wildcard $(SRCDIR)/thirdparty/libopal/include/opal/AsyncApi.h),)
	LIB_CFLAGS  += -I $(SRCDIR)/thirdparty/libopal/include/opal/
	LIB_LDFLAGS += -L/lib/i386-linux-gnu/ -L/usr/lib/i386-linux-gnu/ -L$(SRCDIR)/thirdparty/libopal/
	LIB_LDLIBS  += -lOpalAsyncApiCore -lOpalCore -lOpalUtils -lirc
	LIB_NODES   += opal

	# libOpalAsyncApi is a 32bit library. So we need to build everything in 32bit
	CFLAGS      += -m32
	LDFLAGS     += -m32
endif
endif

# Enable Socket node type when libnl3 is available
ifeq ($(WITH_NODE_SOCKET),1)
	LIB_NODES   += socket
	WITH_IO      = 1

	# libnl3 is optional but required for network emulation and IRQ pinning
	ifeq ($(shell $(PKGCONFIG) libnl-route-3.0; echo $$?),0)
		LIB_SRCS    += $(addprefix lib/kernel/, nl.c tc.c tc_netem.c if.c)
		LIB_PKGS    += libnl-route-3.0
	endif
endif

# Enable nanomsg node type when libnanomsg is available
ifeq ($(WITH_NODE_NANOMSG),1)
ifeq ($(shell $(PKGCONFIG) nanomsg; echo $$?),0)
	LIB_PKGS    += nanomsg
	LIB_NODES   += nanomsg
	WITH_IO      = 1
else ifeq ($(shell $(PKGCONFIG) libnanomsg; echo $$?),0)
	LIB_PKGS    += libnanomsg
	LIB_NODES   += nanomsg
	WITH_IO      = 1
endif
endif

# Enable ZeroMQ node type when libzmq is available
ifeq ($(WITH_NODE_ZEROMQ),1)
ifeq ($(shell $(PKGCONFIG) libzmq; echo $$?),0)
	LIB_PKGS    += libzmq
	LIB_NODES   += zeromq
	WITH_IO      = 1
endif
endif

# Enable NGSI support
ifeq ($(WITH_NODE_NGSI),1)
ifeq ($(shell $(PKGCONFIG) libcurl; echo $$?),0)
	LIB_PKGS    += libcurl
	LIB_NODES   += ngsi
endif
endif

# Enable WebSocket support
ifeq ($(WITH_NODE_WEBSOCKET),1)
ifeq ($(shell $(PKGCONFIG) libwebsockets; echo $$?),0)
	LIB_PKGS    += libwebsockets
	LIB_NODES   += websocket
	WITH_IO      = 1
	WITH_WEB     = 1
endif
endif

# Enable AMQP support
ifeq ($(WITH_NODE_AMQP),1)
ifeq ($(shell $(PKGCONFIG) librabbitmq; echo $$?),0)
	LIB_PKGS    += librabbitmq
	LIB_NODES   += amqp
	WITH_IO      = 1
endif
endif

# Enable MQTT support
ifeq ($(WITH_NODE_MQTT),1)
ifneq ($(wildcard /usr/include/mosquitto.h),)
	LIB_SRCS    += lib/nodes/mqtt.c
	LIB_CFLAGS  += -DWITH_MQTT
	LIB_LDLIBS  += -lmosquitto
endif
endif
