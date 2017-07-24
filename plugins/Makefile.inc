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

# Plugins
PLUGINS = $(BUILDDIR)/simple_circuit.so \
	  $(BUILDDIR)/example_hook.so

PLUGIN_CFLAGS = -fPIC -DVILLAS -I../include/villas

# Dependencies for plugins
$(BUILDDIR)/example_hook.so:   $(BUILDDIR)/plugins/hooks/example_hook.o
$(BUILDDIR)/simple_circuit.so: $(BUILDDIR)/plugins/models/simple_circuit.o

plugins: $(PLUGINS)

# Compile
$(BUILDDIR)/plugins/%.o: plugins/%.c | $$(dir $$@)
	$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) -c $< -o $@

# Link
$(PLUGINS):
	$(CC) $(LIB_LDFLAGS) -o $@ $^ $(LIB_LDLIBS)

# Plugins are not installed to the system for now...
install-plugins: plugins
	install -D -t $(DESTDIR)$(PREFIX)/share/villas/node/plugins $(PLUGINS)

clean-plugins:
	rm -rf $(BUILDDIR)/plugins $(PLUGINS)

.PHONY: plugins install-plugins clean-plugins