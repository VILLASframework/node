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

LIBEXT_NAME = libvillas-ext
LIBEXT_ABI_VERSION = 1
LIBEXT = $(BUILDDIR)/$(LIBEXT_NAME).so.$(LIBEXT_ABI_VERSION)

LIBEXT_SRCS += $(addprefix lib/, sample.c queue.c queue_signalled.c \
                 memory.c log.c shmem.c utils.c kernel/kernel.c list.c \
                 timing.c pool.c \
               )

LIBEXT_LDFLAGS = -shared
LIBEXT_LDLIBS = -ldl -lrt -Wl,-soname,$(LIBEXT_NAME).so.$(LIBEXT_ABI_VERSION)

LIBEXT_OBJS = $(patsubst %.c, $(BUILDDIR)/%.o, $(LIBEXT_SRCS))

$(LIBEXT_NAME): $(LIBEXT)

$(LIBEXT): $(LIBEXT_OBJS)
	$(CC) $(LIBEXT_LDFLAGS) -o $@ $^ $(LIBEXT_LDLIBS)
	ln -srf $@ $(BUILDDIR)/$(LIBEXT_NAME).so

install-libvillas-ext: libvillas-ext
	install -m 0755 -D -T $(LIBEXT) $(DESTDIR)$(PREFIX)/lib/$(LIBEXT_NAME).so.$(LIBEXT_ABI_VERSION)
	ln -srf $(DESTDIR)$(PREFIX)/lib/$(LIBEXT_NAME).so.$(LIBEXT_ABI_VERSION) $(DESTDIR)$(PREFIX)/lib/$(LIBEXT_NAME).so

clean-libvillas-ext:
	rm -rf $(BUILDDIR)/lib $(LIBEXT)

.PHONY: install-libvillas-ext clean-libvillas-ext $(LIBEXT_NAME)
