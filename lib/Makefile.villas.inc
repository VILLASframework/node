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
LIB_SRCS += $(addprefix lib/kernel/, kernel.c rt.c) \
            $(addprefix lib/, sample.c path.c node.c hook.c log.c log_config.c \
               utils.c super_node.c hist.c timing.c pool.c list.c queue.c \
               queue_signalled.c memory.c advio.c plugin.c node_type.c stats.c \
               mapping.c shmem.c config_helper.c crypt.c compat.c \
               log_helper.c task.c buffer.c table.c bitset.c signal.c \
            )

LIB_LDFLAGS += -shared
LIB_LDLIBS  += $(LDLIBS)

ifeq ($(PLATFORM),Linux)
	LIB_LDLIBS += -ldl -lrt -Wl,-soname,$(LIB_NAME).so.$(LIB_ABI_VERSION)
endif

LIB_PKGS += openssl libcurl

include lib/nodes/Makefile.inc

ifeq ($(WITH_IO),1)
	LIB_SRCS += lib/io.c lib/format_type.c
	include lib/formats/Makefile.inc
endif

ifeq ($(WITH_HOOKS),1)
	LIB_SRCS += lib/hook.c lib/hook_type.c
	include lib/hooks/Makefile.inc
endif

ifeq ($(WITH_WEB),1)
	LIB_SRCS += lib/web.c
	LIB_PKGS += libwebsockets
endif

ifeq ($(WITH_API),1)
	LIB_SRCS += lib/api.c
	include lib/api/Makefile.inc
endif

LIB_SRCS += $(patsubst %, lib/nodes/%.c, $(LIB_NODES))
LIB_SRCS += $(patsubst %, lib/formats/%.c, $(LIB_FORMATS))

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
install-libvillas: libvillas | $(DESTDIR)$(PREFIX)/include/villas/
	install -m 0755 -D -T $(LIB) $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME).so.$(LIB_ABI_VERSION)
	install -m 0644 -D -t $(DESTDIR)$(PREFIX)/include/villas/ include/villas/*.h
	ln -srf $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME).so.$(LIB_ABI_VERSION) $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME).so
	if [ "$(PLATFORM)" == "Linux" ]; then ldconfig; fi

clean-libvillas:
	rm -f $(LIB)

.PHONY: install-libvillas clean-libvillas $(LIB_NAME)
