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
# @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
#   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
#   Unauthorized copying of this file, via any medium is strictly prohibited.
#################################################################################

# Project modules
MODULES = lib plugins src tests

# Default prefix for install target
PREFIX ?= /usr/local

# Default out-of-source build path
BUILDDIR ?= build

# Default debug level for executables
V ?= 2

# Common flags
LDLIBS   =
CFLAGS  += -std=c11 -Iinclude -Iinclude/villas -I. -MMD -mcx16
CFLAGS  += -Wall -Werror -fdiagnostics-color=auto
CFLAGS  += -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE=1 -DV=$(V)
LDFLAGS += -L$(BUILDDIR)

ifdef CI
	CFLAGS += -D_GIT_REV='"${CI_BUILD_REF:0:7}~ci"'
else ifdef GIT
	CFLAGS += -D_GIT_REV='"$(shell git rev-parse --short HEAD)"'
endif

ifdef COVERAGE
	CFLAGS  += -fprofile-arcs -ftest-coverage
	LDFLAGS += --coverage
	LDLIBS  += -lgcov
	
	LIB_LDFLAGS += --coverage
	LIB_LDLIBS += -gcov
endif

# We must compile without optimizations for gcov!
ifneq ($(or $(DEBUG),$(COVERAGE)),)
	CFLAGS += -O0 -g
else
	CFLAGS += -O3
endif

# Build variant
ifdef COVERAGE
	BUILDDIR := $(BUILDDIR)/coverage
else ifdef DEBUG
	BUILDDIR := $(BUILDDIR)/debug
else
	BUILDDIR := $(BUILDDIR)/release
endif

# pkg-config dependencies
PKGS = libconfig

######## Targets ########

# Add flags by pkg-config
CFLAGS += $(addprefix -DWITH_, $(shell echo ${PKGS} | tr a-z- A-Z_ | tr -dc ' A-Z0-9_' ))
CFLAGS += $(shell pkg-config --cflags ${PKGS})
LDLIBS += $(shell pkg-config --libs ${PKGS})

# Default target: build everything; no tests, docs
all: $(MODULES)
	
everything:
	$(MAKE) DEBUG=1
	$(MAKE) COVERAGE=1
	$(MAKE) doc
	$(MAKE) tests
	
.PHONY: all clean install docker doc $(MODULES)
.SECONDARY:
.SECONDEXPANSION:

# Create non-existent directories in build directory
$(BUILDDIR)/%/:
	mkdir -p $@

install: $(addprefix install-,$(MODULES))
	install -m 0755 tools/villas.sh 	   $(PREFIX)/bin/villas

clean:
	rm -rf $(BUILDDIR)

docker:
	docker build -t villas .
	docker run -it -p 80:80 -p 443:443 -p 1234:1234 --privileged --cap-add sys_nic --ulimit memlock=1073741824 --security-opt seccomp:unconfined -v $(PWD):/villas villas

doc:
	( cat Doxyfile ; echo "OUTPUT_DIRECTORY=$(BUILD)/doc/" ) | doxygen -

-include $(wildcard $(BUILDDIR)/**/*.d)
$(foreach MODULE,$(MODULES),$(eval -include $(MODULE)/Makefile.inc))