TARGETS = server send random receive test

# Common dependencies for all binaries
OBJS = if.o utils.o msg.o node.o cfg.o tc.o hooks.o list.o path.o hist.o socket.o file.o 

VPATH = src

# Default debug level
V ?= 2

# Compiler and linker flags
LDLIBS = -pthread -lrt -lm -lconfig
CFLAGS  = -std=c99 -Iinclude/ -MMD -Wall
CFLAGS += -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -DV=$(V)
CFLAGS += -D__GIT_REV__='"-$(shell git rev-parse --short HEAD)"'

# Conditional flags
ifdef DEBUG
	CFLAGS += -O0 -g
else
	CFLAGS += -O3
endif

# Enable OPAL-RT Asynchronous Process support
#OPALDIR = /usr/opalrt/common
OPALDIR = ../opal
ifneq (,$(wildcard $(OPALDIR)/include_target/AsyncApi.h))
	CFLAGS  += -m32 -DENABLE_OPAL_ASYNC -I$(OPALDIR)/include_target
	LDFLAGS += -m32
	LDLIBS  += $(addprefix $(OPALDIR)/lib/redhawk/, libOpalAsyncApiCore.a libOpalCore.a libOpalUtils.a libirc.a)
	COMMON  += opal.o
endif

.PHONY: all clean

# Default target: build everything
all: $(TARGETS)

# Dependencies for individual binaries
server: server.o $(OBJS) 
send: send.o $(OBJS)
receive: receive.o $(OBJS)
random: random.o $(OBJS)
test: test.o $(OBJS)

clean:
	$(RM) *~ *.o *.d
	$(RM) $(TARGETS)

# Include auto-generated dependencies
-include $(wildcard *.d)
