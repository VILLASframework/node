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

rpm: rpm-villas-node rpm-libwebsockets rpm-libxil

rpm-villas-node: $(TAR_VILLAS) $(SPEC_VILLAS) | $(RPMDIR)/SOURCES/
	cp $(TAR_VILLAS) $(RPMDIR)/SOURCES
	rpmbuild -ba --define="_topdir $$(pwd)/$(RPMDIR)" $(BUILDDIR)/packaging/rpm/villas-node.spec

rpm-libxil: $(BUILDDIR)/thirdparty/libxil/ | $(RPMDIR)/SOURCES/
	cmake -DCMAKE_INSTALL_PREFIX:PATH=$(PREFIX) \
	      -H$(SRCDIR)/thirdparty/libxil \
	      -B$(BUILDDIR)/thirdparty/libxil $(CMAKE_OPTS)
	make -C$(BUILDDIR)/thirdparty/libxil package_source
	cp $(BUILDDIR)/thirdparty/libxil/libxil-*.tar.gz $(RPMDIR)/SOURCES/
	rpmbuild -ba --define="_topdir $$(pwd)/$(RPMDIR)" $(SRCDIR)/thirdparty/libxil/libxil.spec

rpm-nanomsg: $(BUILDDIR)/thirdparty/nanomsg/ | $(RPMDIR)/SOURCES/
	cmake -DCMAKE_INSTALL_PREFIX:PATH=$(PREFIX) \
		      -H$(SRCDIR)/thirdparty/nanomsg \
		      -B$(BUILDDIR)/thirdparty/nanomsg $(CMAKE_OPTS)
	make -C$(BUILDDIR)/thirdparty/nanomsg package_source
	cp $(BUILDDIR)/thirdparty/nanomsg/nanomsg-*.tar.gz $(RPMDIR)/SOURCES/
	rpmbuild -ba --define="_topdir $$(pwd)/$(RPMDIR)" $(SRCDIR)/packaging/rpm/nanomsg.spec

rpm-libwebsockets: CMAKE_OPTS += -DLWS_IPV6=1 -DLWS_WITH_STATIC=0 -DLWS_WITHOUT_TESTAPPS=1 -DLWS_WITH_HTTP2=1 -DLWS_WITH_SERVER_STATUS=1
rpm-libwebsockets: | $(RPMDIR)/RPMS/x86_64/ $(BUILDDIR)/thirdparty/libwebsockets/
	cmake -DCMAKE_INSTALL_PREFIX:PATH=$(PREFIX) \
	      -H$(SRCDIR)/thirdparty/libwebsockets \
	      -B$(BUILDDIR)/thirdparty/libwebsockets $(CMAKE_OPTS)
	make  -C$(BUILDDIR)/thirdparty/libwebsockets libwebsockets_rpm
	mv $(BUILDDIR)/thirdparty/libwebsockets/RPM/RPMS/x86_64/libwebsockets-*.rpm $(RPMDIR)/RPMS/x86_64/

# We patch version number and release fields of the spec file based on the current Git commit
$(SPEC_VILLAS): packaging/rpm/villas-node.spec | $$(dir $$@)
	sed -e "s/§VERSION§/$(VERSION_NUM)/g" \
	    -e "s/§RELEASE§/$(RELEASE)/g" < $^ > $@

sign-rpm:
	rpmsign $(RPMDIR)/RPMS/*/.rpm

clean-rpm:
	rm -rf $(RPMDIR)

.PHONY: rpm clean-rpm rpm-libwebsockets rpm-libxil $(SPEC_VILLAS)
