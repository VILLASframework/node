TARGETS = server send random receive test

# Common objs
OBJS  = path.o node.o hooks.o msg.o cfg.o
# Helper libs
OBJS += utils.o list.o hist.o log.o timing.o
# Node types
OBJS += file.o socket.o if.o tc.o

VPATH = src

# Default debug level
V ?= 2

# Compiler and linker flags
LDLIBS = -pthread -lrt -lm -lconfig
CFLAGS  = -std=gnu99 -Iinclude/ -MMD -Wall -O3
CFLAGS += -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -DV=$(V)

# Add more compiler flags
ifdef DEBUG
	CFLAGS += -O0 -g
endif
ifneq (,$(shell which git))
	CFLAGS += -D_GIT_REV='"$(shell git rev-parse --short HEAD)"'
endif

# Enabled GTFPGA support when libpci is available
ifneq (,$(wildcard /usr/include/pci/pci.h))
	CFLAGS += -DENABLE_GTFPGA
	LDLIBS += -lpci
	OBJS   += gtfpga.o
endif

# Enable OPAL-RT Asynchronous Process support
OPALDIR = /usr/opalrt/common
#OPALDIR = ../opal
ifneq (,$(wildcard $(OPALDIR)/include_target/AsyncApi.h))
	override CFLAGS  += -m32 -DENABLE_OPAL_ASYNC -I$(OPALDIR)/include_target
	override LDFLAGS += -m32 -Wl,-L/lib/i386-linux-gnu/
	override LDLIBS  += $(addprefix $(OPALDIR)/lib/redhawk/, libOpalAsyncApiCore.a libOpalCore.a libOpalUtils.a libirc.a)
	override OBJS    += opal.o
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

protected: CFLAGS += -DPROTECT -DVALID_UNTIL=$(shell date -d "now +5months" +%s) -s -O3 -fvisibility=hidden
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
