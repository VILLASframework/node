## Main project Makefile
#
# The build system of this project is based on GNU Make and pkg-config
#
# To retain maintainability, the project is divided into multiple modules.
# Each module has its own Makefile which gets included.
#
# Please read "Recursive Make Considered Harmful" from Peter Miller
#  to understand the motivation for this structure.
#
# [1] http://aegis.sourceforge.net/auug97.pdf
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

# Project modules
MODULES = clients lib plugins src tests thirdparty tools packaging doc etc web

# Default prefix for install target
PREFIX ?= /usr/local

# Default out-of-source build path
BUILDDIR ?= build

# Default debug level for executables
V ?= 2

# Common flags
LDLIBS   =
CFLAGS  += -I. -Iinclude -Iinclude/villas
CFLAGS  +=  @$(BUILDDIR)/defines
CFLAGS  += -std=c11 -MMD -mcx16 
CFLAGS  += -Wall -Werror -fdiagnostics-color=auto
LDFLAGS += -L$(BUILDDIR)

# Some tools
PKGCONFIG := PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/lib/pkgconfig:$(PKG_CONFIG_PATH) pkg-config
SHELL := bash

# We must compile without optimizations for gcov!
ifdef DEBUG
	CFLAGS += -O0 -g
	VARIANTS += debug
else
	CFLAGS += -O3
	VARIANTS += release
endif

ifdef PROFILE
	CFLAGS += -pg
	LDFLAGS += -pg
	
	VARIANTS += profile
endif

ifdef COVERAGE
	CFLAGS  += -fprofile-arcs -ftest-coverage
	LDFLAGS += --coverage
	LDLIBS  += -lgcov
	
	LIB_LDFLAGS += -coverage
	LIB_LDLIBS += -gcov
	
	VARIANTS += coverage
endif

EMPTY :=
SPACE := $(EMPTY) $(EMPTY)

VARIANT = $(subst $(SPACE),-,$(strip $(VARIANTS)))
BUILDDIR := $(BUILDDIR)/$(VARIANT)
SRCDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Add git revision and build variant defines
VERSION := $(shell git describe --tags --abbrev=0 --match v*)
VERSION_NUM := $(shell VERSION=$(VERSION); echo $${VERSION:1})

ifdef CI
	VARIANT := $(VARIANT)-ci
endif

GIT_REV    := $(shell git rev-parse --short=7  HEAD)
GIT_BRANCH := $(shell git symbolic-ref --short HEAD)

# pkg-config dependencies
PKGS = libconfig

######## Targets ########

# Add flags by pkg-config
CFLAGS += $(shell $(PKGCONFIG) --cflags ${PKGS})
LDLIBS += $(shell $(PKGCONFIG) --libs ${PKGS})

all: src plugins tools
src plugins tools tests: lib

# Build all variants: debug, coverage, ...
everything:
	$(MAKE) RELEASE=1
	$(MAKE) DEBUG=1
	$(MAKE) COVERAGE=1
	$(MAKE) PROFILE=1

# Create non-existent directories
.SECONDEXPANSION:
.PRECIOUS: %/
%/:
	mkdir -p $@

define DEFINES
-DV=$(V)

-DPLUGIN_PATH=\"$(PREFIX)/share/villas/node/plugins\"
-DWEB_PATH=\"$(PREFIX)/share/villas/node/web\"
-DSYSFS_PATH=\"/sys\"
-DPROCFS_PATH=\"/proc\"
-DBUILDID=\"$(VERSION)-$(GIT_REV)-$(VARIANT)\"

-D_POSIX_C_SOURCE=200809L
-D_GNU_SOURCE=1
endef
export DEFINES

$(BUILDDIR)/defines: | $$(dir $$@)
	echo "$${DEFINES}" > $@
	echo -e "$(addprefix \n-DWITH_, $(shell echo ${PKGS} | tr a-z- A-Z_ | tr -dc ' A-Z0-9_' ))" >> $@
	echo -e "$(addprefix \n-DWITH_, $(shell echo ${LIB_PKGS} | tr a-z- A-Z_ | tr -dc ' A-Z0-9_' ))" >> $@

install: $(addprefix install-,$(filter-out thirdparty doc clients,$(MODULES)))
clean: $(addprefix clean-,$(filter-out thirdparty doc clients,$(MODULES)))

.PHONY: all everything clean install

-include $(wildcard $(BUILDDIR)/**/*.d)
-include $(addsuffix /Makefile.inc,$(MODULES))
