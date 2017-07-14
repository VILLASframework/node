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

LIB_NAME = libvillas
LIB_ABI_VERSION = 1
LIB = $(BUILDDIR)/$(LIB_NAME).so.$(LIB_ABI_VERSION)

# Object files for libvillas
LIB_SRCS += $(addprefix lib/nodes/, file.c cbuilder.c shmem.c signal.c loopback.c) 		\
            $(addprefix lib/kernel/, kernel.c rt.c)		\
            $(addprefix lib/, sample.c path.c node.c hook.c	\
               log.c log_config.c utils.c super_node.c hist.c timing.c pool.c	\
               list.c queue.c queue_signalled.c memory.c advio.c web.c api.c	\
               plugin.c node_type.c stats.c mapping.c sample_io.c shmem.c \
               config_helper.c sample_io_json.c crypt.c compat.c log_table.c \
               log_helper.c \
            )

LIB_LDFLAGS = -shared
LIB_LDLIBS  = $(LDLIBS) -ldl -lrt -Wl,-soname,$(LIB_NAME).so.$(LIB_ABI_VERSION)

LIB_PKGS += libwebsockets openssl

######## Node types ########

# Enable Socket node type when libnl3 is available
ifndef WITHOUT_SOCKET
ifeq ($(shell $(PKGCONFIG) libnl-route-3.0; echo $$?),0)
	LIB_SRCS    += $(addprefix lib/nodes/, socket.c)
	LIB_SRCS    += $(addprefix lib/kernel/, nl.c tc.c if.c)
	LIB_SRCS    += $(addprefix lib/, msg.c)
	LIB_PKGS    += libnl-route-3.0
endif
endif

# Enable nanomsg node type when libnanomsg is available
ifndef WITHOUT_NANOMSG
ifeq ($(shell $(PKGCONFIG) nanomsg; echo $$?),0)
	LIB_SRCS    += lib/nodes/nanomsg.c
	LIB_PKGS    += nanomsg
else ifeq ($(shell $(PKGCONFIG) libnanomsg; echo $$?),0)
	LIB_SRCS    += lib/nodes/nanomsg.c
	LIB_PKGS    += libnanomsg
endif
endif

# Enable ZeroMQ node type when libzmq is available
ifndef WITHOUT_ZMQ
ifeq ($(shell $(PKGCONFIG) libzmq; echo $$?),0)
	LIB_SRCS    += $(addprefix lib/nodes/, zeromq.c)
	LIB_PKGS    += libzmq
endif
endif

# Enable VILLASfpga support when libxil is available
ifndef WITHOUT_FPGA
ifeq ($(shell $(PKGCONFIG) libxil; echo $$?),0)
	LIB_SRCS    += $(addprefix lib/nodes/, fpga.c) \
	               $(addprefix lib/kernel/, pci.c vfio.c) \
	               $(wildcard  lib/fpga/*.c) \
	               $(wildcard  lib/fpga/ips/*.c)

	LIB_PKGS    += libxil
endif
endif

# Enable NGSI support
ifndef WITHOUT_NGSI
ifeq ($(shell $(PKGCONFIG) libcurl jansson; echo $$?),0)
	LIB_SRCS    += lib/nodes/ngsi.c
	LIB_PKGS    += libcurl jansson
endif
endif

# Enable WebSocket support
ifeq ($(shell $(PKGCONFIG) libwebsockets jansson; echo $$?),0)
	LIB_SRCS    += lib/nodes/websocket.c lib/webmsg.c
	LIB_PKGS    += libwebsockets jansson
endif

# Enable OPAL-RT Asynchronous Process support (will result in 32bit binary!!!)
ifdef WITH_OPAL
ifneq ($(wildcard thirdparty/libopal/include/opal/AsyncApi.h),)
	LIB_OBJS    += opal.o

	LIB_CFLAGS  += -I thirdparty/libopal/include/opal/
	LIB_LDFLAGS += -L/lib/i386-linux-gnu/ -L/usr/lib/i386-linux-gnu/ -Lthirdparty/libopal/
	LIB_LDLIBS  += -lOpalAsyncApiCore -lOpalCore -lOpalUtils -lirc

	# libOpalAsyncApi is a 32bit library. So we need to build everything in 32bit
	CFLAGS      += -m32
	LDFLAGS     += -m32
	BUILDDIR    := $(BUILDDIR)
endif
endif

# Add flags by pkg-config
LIB_LDLIBS += $(shell $(PKGCONFIG) --libs $(LIB_PKGS))
LIB_CFLAGS += $(shell $(PKGCONFIG) --cflags $(LIB_PKGS))

LIB_OBJS = $(patsubst %.c, $(BUILDDIR)/%.o, $(LIB_SRCS))

$(LIB_NAME): $(LIB)

# Link
$(LIB): $(LIB_OBJS)
	$(CC) $(LIB_LDFLAGS) -o $@ $^ $(LIB_LDLIBS)
	ln -srf $@ $(BUILDDIR)/$(LIB_NAME).so

# Install
install-libvillas: libvillas
	install -m 0755 -D -T $(LIB) $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME).so.$(LIB_ABI_VERSION)
	install -m 0644 -D -t $(DESTDIR)$(PREFIX)/include/villas/ include/villas/*.h
	ln -srf $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME).so.$(LIB_ABI_VERSION) $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME).so
	ldconfig

clean-libvillas:
	rm -rf $(BUILDDIR)/lib $(LIB)

.PHONY: install-libvillas clean-libvillas $(LIB_NAME)
