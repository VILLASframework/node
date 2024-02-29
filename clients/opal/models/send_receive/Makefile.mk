# Makefile.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

TARGET = AsyncIP

VPATH = src

RTLAB_INTEL_COMPILER ?= 1

# Compiler selection
ifeq ($(RTLAB_INTEL_COMPILER),1)
	CC = opicc
	LD = opicpc
else
	CC = gcc
	LD = g++

	INTEL_LIBS = -limf -lirc
	INTEL_OBJS = compat.o
endif

# Support for debugging symbols
ifeq ($(DEBUG),1)
	CC_DEBUG_OPTS = -g -D_DEBUG
	LD_DEBUG_OPTS = -g
else
	CC_DEBUG_OPTS = -O
	LD_DEBUG_OPTS =
endif

TARGET_LIB = -lpthread -lm -ldl -lutil -lrt $(INTEL_LIBS)

LIBPATH  = -L.           $(OPAL_LIBPATH)
INCLUDES = -I. -Iinclude $(OPAL_INCPATH)

ifneq ($(RTLAB_ROOT),)
	INCLUDES += -I$(RTLAB_ROOT)/common/include_target
endif

CC_OPTS  = -std=c99 -D_GNU_SOURCE -MMD
LD_OPTS  =
OBJS     = main.o msg.o utils.o socket.o $(INTEL_OBJS)

ifneq ($(PROTOCOL),)
	CC_OPTS += -DPROTOCOL=$(PROTOCOL)
endif

ADDLIB   = -lOpalCore -lOpalUtils
LIBS     = -lOpalAsyncApiCore $(ADDLIB) $(TARGET_LIB) $(OPAL_LIBS)

CFLAGS   = -c $(CC_OPTS) $(CC_DEBUG_OPTS) $(INCLUDES)
LDFLAGS  = $(LD_OPTS) $(LD_DEBUG_OPTS) $(LIBPATH)

all: $(TARGET)

install: $(TARGET)
	install -m 0755 -D -t $(DESTDIR)$(PREFIX)/bin $(TARGET)

clean:
	rm -f $(OBJS) $(OBJS:%.o=%.d) $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

-include $(wildcard *.d)
