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

CC_OPTS  = -m32 -std=c99 -D_GNU_SOURCE -MMD
LD_OPTS  = -m32
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