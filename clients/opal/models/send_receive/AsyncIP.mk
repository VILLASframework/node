# Makefile.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

# Specify program name
PROGRAM = AsyncIP

TARGET_RTLAB_ROOT ?= /usr/opalrt
RTLAB_ROOT = $(TARGET_RTLAB_ROOT)

# Intel is the default compiler
RTLAB_INTEL_COMPILER ?= 0
ifeq ($(RTLAB_INTEL_COMPILER),1)
	CC = opicc
else
	CC = gcc
endif

LIB_TARGET = -lpthread -lrt -lm

# Add Intel C library for compilation with gcc
ifeq ($(RTLAB_INTEL_COMPILER),0)
	LIB_TARGET += -lirc -ldl
else
	LD_OPTS += -diag-disable remark,warn
endif

LD := $(CC)

CC_OPTS  = -std=c99 -D_GNU_SOURCE -MMD
LD_OPTS  =
OBJS     = main.o msg.o utils.o socket.o

ifeq ($(DEBUG),1)
	CC_OPTS += -g -D_DEBUG
	LD_OPTS += -g
else
	CC_OPTS += -O
endif

ifneq ($(PROTOCOL),)
	CC_OPTS += -DPROTOCOL=$(PROTOCOL)
endif

INCPATH = -I. -I$(RTLAB_ROOT)/common/include_target
LIBPATH = -L. $(OPAL_LIBPATH)

# The required libraries are transfered automatically in the model directory before compilation
LIBS := -lOpalAsyncApiCore -lOpalCore -lOpalUtils $(OPAL_LIBS) $(LIB_TARGET)
CFLAGS	= $(CC_OPTS) $(INCPATH)
LDFLAGS = $(LD_OPTS) $(LIBPATH)

all: $(PROGRAM)

install:
	\mkdir -p $(RTLAB_ROOT)/local
	\chmod 755 $(RTLAB_ROOT/local
	\cp -f $(PROGRAM) $(RTLAB_ROOT)/local

clean:
	\rm -f $(OBJS) $(PROGRAM)

$(PROGRAM): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) -Wl,--start-group $(LIBS) -Wl,--end-group
