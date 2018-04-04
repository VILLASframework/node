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

RPMDIR = $(BUILDDIR)/packaging/rpm

SPEC_VILLAS = $(BUILDDIR)/packaging/rpm/villas-node.spec

rpm: rpm-villas-node

rpm-villas-node: $(TAR_VILLAS) $(SPEC_VILLAS) | $(RPMDIR)/SOURCES/
	cp $(TAR_VILLAS) $(RPMDIR)/SOURCES
	rpmbuild -ba --define="_topdir $$(pwd)/$(RPMDIR)" $(BUILDDIR)/packaging/rpm/villas-node.spec

# We patch version number and release fields of the spec file based on the current Git commit
$(SPEC_VILLAS): packaging/rpm/villas-node.spec | $$(dir $$@)
	sed -e "s/§VERSION§/$(VERSION_NUM)/g" \
	    -e "s/§RELEASE§/$(RELEASE)/g" < $^ > $@

sign-rpm:
	rpmsign $(RPMDIR)/RPMS/*/.rpm

clean-rpm:
	rm -rf $(RPMDIR)

.PHONY: rpm clean-rpm rpm-libwebsockets rpm-libxil $(SPEC_VILLAS)
