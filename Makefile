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
MODULES = lib thirdparty packaging doc etc web tests

# Modules which are not included in default, install and clean targets
MODULES_EXCLUDE = thirdparty packaging doc

# Default prefix for install target
PREFIX ?= /usr/local

# Default out-of-source build path
BUILDDIR ?= build

# Default debug level for executables
V ?= 2

# Platform
PLATFORM ?= $(shell uname)

include Makefile.config

ifeq ($(WITH_SRC),1)
	MODULES += src
endif
	
ifeq ($(WITH_TOOLS),1)
	MODULES += tools
endif

ifeq ($(WITH_PLUGINS),1)
	MODULES += plugins
endif

ifeq ($(WITH_TESTS),1)
	MODULES += tests
endif

# Common flags
LDLIBS   =
CFLAGS  += -std=c11 -MMD -mcx16 -I$(BUILDDIR)/include -I$(SRCDIR)/include
CFLAGS  += -Wall -Werror -fdiagnostics-color=auto -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE=1

LDFLAGS += -L$(BUILDDIR)

# Some tools
PKG_CONFIG_PATH := $(PKG_CONFIG_PATH):/opt/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib/pkgconfig

PKGCONFIG := PKG_CONFIG_PATH=:$(PKG_CONFIG_PATH) pkg-config
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
	CFLAGS  += --coverage
	LDLIBS += -lgcov

	VARIANTS += coverage
endif

EMPTY :=
SPACE := $(EMPTY) $(EMPTY)

VARIANT = $(shell uname -s)-$(shell uname -m)-$(subst $(SPACE),-,$(strip $(VARIANTS)))
BUILDDIR := $(BUILDDIR)/$(VARIANT)
SRCDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

VPATH = $(SRCDIR)

# Add git revision and build variant defines
VERSION := $(shell git describe --tags --abbrev=0 --match 'v*')
VERSION_NUM := $(shell VERSION=$(VERSION); echo $${VERSION:1})

export BUILDDIR VARIANT VERSION VERSION_NUM

ifdef CI
	VARIANT := $(VARIANT)-ci

	GIT_REV    := $(shell echo $${CI_COMMIT_SHA:0:7})
	GIT_BRANCH := $(CI_COMMIT_REF_NAME)
	
	ifdef CI_COMMIT_TAG
		RELEASE = 1
	else
		RELEASE = 1.$(subst -,_,$(CI_COMMIT_REF_NAME))_$(subst -,_,$(VARIANT)).$(shell date +%Y%m%d)git$(GIT_REV)
	endif
else
	GIT_REV    := $(shell git rev-parse --short=7    HEAD)
	GIT_BRANCH := $(shell git rev-parse --abbrev-ref HEAD)
	
	RELEASE = 1.$(subst -,_,$(GIT_BRANCH))_$(subst -,_,$(VARIANT)).$(shell date +%Y%m%d)git$(GIT_REV)
endif

BUILDID = "$(VERSION)-$(GIT_REV)-$(VARIANT)"

# pkg-config dependencies
PKGS = openssl jansson

ifeq ($(WITH_CONFIG),1)
	PKGS += libconfig
endif

######## Targets ########

# Add flags by pkg-config
CFLAGS += $(shell $(PKGCONFIG) --cflags ${PKGS})
LDLIBS += $(shell $(PKGCONFIG) --libs ${PKGS})

all:                          $(filter-out $(MODULES_EXCLUDE),$(MODULES))
install: $(addprefix install-,$(filter-out $(MODULES_EXCLUDE),$(MODULES)))
clean:   $(addprefix clean-,  $(filter-out $(MODULES_EXCLUDE),$(MODULES)))
	
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

escape = $(shell echo $1 | tr a-z- A-Z_ | tr -dc ' A-Z0-9_')

.PHONY: all everything clean install

include $(wildcard $(SRCDIR)/make/Makefile.*)
include $(wildcard $(BUILDDIR)/**/*.d)
include $(patsubst %,$(SRCDIR)/%/Makefile.inc,$(MODULES))
