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

# Executables
TARGETS = $(BUILDDIR)/villas-node \
	  $(BUILDDIR)/villas-pipe  \
	  $(BUILDDIR)/villas-signal \
	  $(BUILDDIR)/villas-test-rtt \
	  $(BUILDDIR)/villas-test-cmp \
	  $(BUILDDIR)/villas-test-shmem \
	  $(BUILDDIR)/villas-hook

SRC_LDLIBS = $(LDLIBS) -pthread -lm -lvillas
SRC_CFLAGS = $(CFLAGS)
SRC_LDFLAGS = $(LDFLAGS) -Wl,-rpath,'$$ORIGIN'

ifeq ($(WITH_WEB),1)
ifeq ($(WITH_WEBSOCKET),1)
	SRC_CFLAGS += -DWITH_WEBSOCKET
endif
endif

src: $(TARGETS)

$(TARGETS): $(BUILDDIR)/villas-%: $(BUILDDIR)/src/%.o

# Compile executable objects
$(BUILDDIR)/src/%.o: src/%.c | $$(dir $$@)
	$(CC) $(SRC_CFLAGS) -c $< -o $@

# Build villas-shmem only with libvillas-ext (to ensure that libext contains everything needed)
$(BUILDDIR)/villas-shmem: $(BUILDDIR)/src/shmem.o libvillas-ext
	$(CC) $(SRC_LDFLAGS) $(BUILDDIR)/src/shmem.o $(filter-out -lvillas,$(SRC_LDLIBS)) -lvillas-ext -o $@

# Link target executables
$(TARGETS): | $(LIBS)
	$(CC) $(SRC_LDFLAGS) $^ $(SRC_LDLIBS) -o $@

# Install
install-src: src
	install -m 0755 -D -t $(DESTDIR)$(PREFIX)/bin $(TARGETS)

clean-src:
	rm -rf $(BUILDDIR)/src $(TARGETS)

.PHONY: src src-tests src-tests
