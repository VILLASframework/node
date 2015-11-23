TARGETS = server send random receive test

# Common objs
OBJS  = path.o node.o hooks.o msg.o cfg.o
# Helper libs
OBJS += utils.o list.o hist.o log.o timing.o checks.o

VPATH = src

# Default debug level
V ?= 2

# Compiler and linker flags
CC = gcc
LDLIBS  = -pthread -lrt -lm -lconfig
override CFLAGS += -std=gnu99 -Iinclude/ -MMD -Wall
override CFLAGS += -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE=1 -DV=$(V)

# Add more compiler flags
ifdef DEBUG
	override CFLAGS += -O0 -g
else
	override CFLAGS += -O3
endif
ifneq (,$(shell which git))
	override CFLAGS += -D_GIT_REV='"$(shell git rev-parse --short HEAD)"'
endif

######## Node types ########

# Enable file node type support
ifndef DISABLE_FILE
	override CFLAGS += -DENABLE_FILE
	OBJS   += file.o
endif

# Enable Socket node type when libnl3 is available
ifeq ($(shell pkg-config libnl-3.0; echo $$?),0)
	override CFLAGS += -DENABLE_SOCKET $(shell pkg-config --cflags libnl-3.0)
	LDLIBS += -lnl-3 -lnl-route-3
	OBJS   += nl.o tc.o if.o socket.o
endif

# Enable GTFPGA support when libpci is available
ifeq ($(shell pkg-config libpci; echo $$?),0)
	override CFLAGS += -DENABLE_GTFPGA $(shell pkg-config --cflags libpci)
	LDLIBS += $(shell pkg-config --libs libpci)
	OBJS   += gtfpga.o
endif

# Enable NGSI support
ifeq ($(shell pkg-config libcurl jansson uuid; echo $$?),0)
	override CFLAGS += -DENABLE_NGSI $(shell pkg-config --cflags libcurl jansson uuid)
	LDLIBS += $(shell pkg-config --libs libcurl jansson uuid)
	OBJS   += ngsi.o
endif

# Enable OPAL-RT Asynchronous Process support (will result in 32bit binary!!!)
ifneq (,$(wildcard $(OPALDIR)/include_target/AsyncApi.h))
	override CFLAGS += -m32 -DENABLE_OPAL_ASYNC
	LDFLAGS += -m32 -Wl,-L/lib/i386-linux-gnu/,-L/usr/lib/i386-linux-gnu/
	LDLIBS  += $(addprefix $(OPALDIR)/lib/redhawk/, libOpalAsyncApiCore.a libOpalCore.a libOpalUtils.a libirc.a)
	OBJS    += opal.o
endif

.PHONY: all clean strip protected

# Default target: build everything
all: $(TARGETS)

# Dependencies for individual binaries
server:  server.o  $(OBJS)
send:    send.o    $(OBJS)
receive: receive.o $(OBJS)
test:    test.o    $(OBJS)
random:  random.o  msg.o utils.o timing.o log.o


clean:
	$(RM) *~ *.o *.d
	$(RM) $(TARGETS)

# Include auto-generated dependencies
-include $(wildcard *.d)
