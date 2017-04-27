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

doc: | $(BUILDDIR)/doc/
	( cat Doxyfile ; echo "OUTPUT_DIRECTORY=$(BUILDDIR)/doc/" ) | doxygen -

install-doc: doc
	mkdir -p $(DESTDIR)$(PREFIX)/share/villas/node/doc/
	cp -R $(BUILDDIR)/doc/html/* $(DESTDIR)$(PREFIX)/share/villas/node/doc/

clean-doc:
	rm -rf $(BUILDDIR)/doc/

.PHONY: doc install-doc clean-doc