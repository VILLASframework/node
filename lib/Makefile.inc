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

SONAMES = villas villas-ext

LIBS = $(patsubst %, lib%, $(SONAMES))

LIB_CFLAGS  = $(CFLAGS) -fPIC

-include lib/hooks/Makefile.inc
-include lib/nodes/Makefile.inc
-include lib/io/Makefile.inc

-include $(patsubst %, lib/Makefile.%.inc, $(SONAMES))

LIB_CFLAGS += $(addprefix -DWITH_, $(call escape,$(LIB_PKGS)))

# Compile
$(BUILDDIR)/lib/%.o: lib/%.c | $$(dir $$@)
	$(CC) $(LIB_CFLAGS) -c $< -o $@

lib: $(patsubst %, lib%, $(SONAMES))

install-lib: $(patsubst %, install-lib%, $(SONAMES))

clean-lib: $(patsubst %, clean-lib%, $(SONAMES))
	rm -rf $(BUILDDIR)/lib

.PHONY: lib install-lib clean-lib
