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

DEPS_CMAKE = libxil libwebsockets libiec61850 criterion jansson nanomsg
DEPS_AUTOCONF = libnl libconfig libcurl libzmq

DEPS = $(DEPS_CMAKE) $(DEPS_AUTOCONF)

ifdef DEBUG
	CMAKE_OPTS += -DCMAKE_BUILD_TYPE=DEBUG

	AC_CPPFLAGS=-DDEBUG
	AC_CFLAGS=-g -O0
	AC_CXXFLAGS=-g -O0
endif

CONFIGURE_OPTS = --prefix=$(PREFIX)

thirdparty:

# Install & compile autotools based projects
$(DEPS_AUTOCONF): CPPFLAGS=$(AC_CPPFLAGS) CFLAGS=$(AC_CFLAGS) CXXFLAGS=$(AC_CXXFLAGS)
$(DEPS_AUTOCONF): | $(BUILDDIR)/thirdparty/$$@/
	autoreconf -fi $(SRCDIR)/thirdparty/$@
	cd $(BUILDDIR)/thirdparty/$@ && $(SRCDIR)/thirdparty/$@/configure $(CONFIGURE_OPTS) && make

# Install & compile CMake based projects
$(DEPS_CMAKE): | $(BUILDDIR)/thirdparty/$$@/
	cmake -DCMAKE_INSTALL_PREFIX:PATH=$(PREFIX) \
	      -H$(SRCDIR)/thirdparty/$@ \
	      -B$(BUILDDIR)/thirdparty/$@ $(CMAKE_OPTS)
	make  -C$(BUILDDIR)/thirdparty/$@

$(addprefix install-,$(DEPS)): install-%: %
	make -C$(BUILDDIR)/thirdparty/$(@:install-%=%) install
	ldconfig

$(addprefix clean-,$(DEPS)):
	rm -rf $(BUILDDIR)/thirdparty/$(@:clean-%=%)

install-thirdparty: $(addprefix install-,$(DEPS))

clean-thirdparty: $(addprefix clean-,$(DEPS))
	rm -rf $(BUILDDIR)/thirdparty

# libconfig's build system is currently broken.
# This is a workaround for: https://github.com/hyperrealm/libconfig/issues/53
libconfig: | libconfig-fix
libconfig-fix:
	rm -f $(SRCDIR)/thirdparty/libconfig/lib/scanner.[hc]

jansson: CMAKE_OPTS += -DJANSSON_BUILD_DOCS=OFF
libiec61850: CMAKE_OPTS += -DBUILD_EXAMPLES=0 -DBUILD_PYTHON_BINDINGS=0
libwebsockets: CMAKE_OPTS += -DLWS_IPV6=1 -DLWS_WITH_STATIC=0 -DLWS_WITHOUT_TESTAPPS=1 -DLWS_WITH_HTTP2=1 -DLWS_WITH_SERVER_STATUS=1
libzmq: CONFIGURE_OPTS += --with-libsodium --with-pgm --enable-drafts

.PHONY: $(DEPS) thirdparty clean-thirdparty install-thirdparty
