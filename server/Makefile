TARGETS = server send random receive test

# Common objs
OBJS  = path.o node.o hooks.o msg.o cfg.o stats.o
# Helper libs
OBJS += utils.o list.o hist.o log.o timing.o checks.o
# Node types
OBJS += file.o

VPATH = src

# Default debug level
V ?= 2

# Compiler and linker flags
CC = gcc
LDLIBS  = -pthread -lrt -lm -lconfig
override CFLAGS += -std=gnu99 -Iinclude/ -MMD -Wall -O3
override CFLAGS += -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE=1 -DV=$(V)

# Default include paths for third-party libs
PCIDIR  ?= /usr/include
NLDIR   ?= /usr/include/libnl3
OPALDIR ?= /usr/opalrt/common
#OPALDIR ?= ../contrib/opal

# Add more compiler flags
ifdef DEBUG
	override CFLAGS += -O0 -g
endif
ifneq (,$(shell which git))
	override CFLAGS += -D_GIT_REV='"$(shell git rev-parse --short HEAD)"'
endif

# Enable Socket node type when libnl3 is available
ifneq (,$(wildcard $(NLDIR)/netlink/netlink.h))
	override CFLAGS += -DENABLE_SOCKET -I$(NLDIR)
	LDLIBS += -lnl-3 -lnl-route-3
	OBJS   += nl.o tc.o if.o socket.o
endif

# Enable GTFPGA support when libpci is available
ifneq (,$(wildcard $(PCIDIR)/pci/pci.h))
	override CFLAGS += -DENABLE_GTFPGA -I$(PCIDIR)
	LDLIBS += -lpci
	OBJS   += gtfpga.o
endif

# Enable OPAL-RT Asynchronous Process support (will result in 32bit binary!!!)
ifneq (,$(wildcard $(OPALDIR)/include_target/AsyncApi.h))
	override CFLAGS += -m32 -DENABLE_OPAL_ASYNC -I$(OPALDIR)/include_target
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

protected: CFLAGS += -DLICENSE -DLICENSE_VALID=$(shell date -d "now +5months" +%s) -s -O3 -fvisibility=hidden
protected: clean strip

strip: $(TARGETS)
	strip	--remove-section=.comment \
		--remove-section=.note \
		--strip-debug \
		--strip-all \
		$(TARGETS) 

clean:
	$(RM) *~ *.o *.d
	$(RM) $(TARGETS)

# Include auto-generated dependencies
-include $(wildcard *.d)
