# Executables
TARGETS = server pipe random test

# Libraries
LIBS = libs2ss.so

# Common objs
OBJS = path.o hooks.o cfg.o utils.o list.o hist.o log.o timing.o

# Object files for libs2ss
LIB_OBJS = msg.o node.o checks.o list.o

# Source directories
VPATH = src lib

# Default debug level
V ?= 2

GIT_REV=$(shell git rev-parse --short HEAD)

# Compiler and linker flags
CC = gcc
LDLIBS  = -pthread -lrt -lm -lconfig -ls2ss

CFLAGS += -std=gnu99 -Iinclude/ -I. -MMD -Wall -D_GIT_REV='"$(GIT_REV)"' -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE=1 -DV=$(V)
LDFLAGS += -Wl,-L.,-rpath,'$$ORIGIN'

# Add more compiler flags
ifdef DEBUG
	CFLAGS += -O0 -g
else
	CFLAGS += -O3
endif

######## Node types ########

# Enable file node type support
ifndef DISABLE_FILE
	LIB_OBJS += file.o
endif

# Enable Socket node type when libnl3 is available
ifeq ($(shell pkg-config libnl-3.0; echo $$?),0)
	LIB_OBJS   += socket.o nl.o tc.o if.o
	LIB_CFLAGS += $(shell pkg-config --cflags libnl-3.0)
	LIB_LDLIBS += -lnl-3 -lnl-route-3 
endif

# Enable GTFPGA support when libpci is available
ifeq ($(shell pkg-config libpci; echo $$?),0)
	LIB_OBJS   += gtfpga.o
	LIB_CFLAGS += $(shell pkg-config --cflags libpci)
	LIB_LDLIBS += $(shell pkg-config --libs libpci)
endif

# Enable NGSI support
ifeq ($(shell pkg-config libcurl jansson uuid; echo $$?),0)
	LIB_OBJS   += ngsi.o
	LIB_CFLAGS += $(shell pkg-config --cflags libcurl jansson uuid)
	LIB_LDLIBS += $(shell pkg-config --libs libcurl jansson uuid)
endif

# Enable OPAL-RT Asynchronous Process support (will result in 32bit binary!!!)
ifneq (,$(wildcard $(OPALDIR)/include_target/AsyncApi.h))
	LIB_OBJS    += opal.o
	LIB_CFLAGS  += -m32 -I$(OPALDIR)/include_target
	LIB_LDFLAGS += -m32 -Wl,-L/lib/i386-linux-gnu/,-L/usr/lib/i386-linux-gnu/,-L$(OPALDIR)/lib/redhawk/
	LIB_LDLIBS  += -lOpalAsyncApiCore -lOpalCore -lOpalUtils -lirc
endif

######## Targets ########

.PHONY: all clean install release docker

# Default target: build everything
all: $(LIBS) $(TARGETS)

# Dependencies for individual binaries
server:  server.o  $(OBJS)
send:    send.o    $(OBJS)
receive: receive.o $(OBJS)
test:    test.o    $(OBJS)
random:  random.o  msg.o utils.o timing.o log.o

# Libraries
libs2ss.so: CFLAGS += -fPIC $(LIB_CFLAGS)
libs2ss.so: $(LIB_OBJS)
	$(CC) $(LIB_LDFLAGS) $(LIB_LDLIBS) -shared -o $@ $^

# Common targets
install: $(TARGETS)
	install -m 0644 libs2ss.so $(PREFIX)/lib
	install -m 0755 server -T $(PREFIX)/bin/s2ss
	install -m 0755 send $(PREFIX)/bin/s2ss-send
	install -m 0755 receive $(PREFIX)/bin/s2ss-receive
	install -m 0755 random $(PREFIX)/bin/s2ss-random
	install -m 0755 test $(PREFIX)/bin/s2ss-test

release: all
	tar czf s2ss-$(COMMIT)-docs.tar.gz documentation/html/
	tar czf s2ss-$(COMMIT).tar.gz server/server server/test server/send server/receive server/random server/etc/
	rsync *.tar.gz $(DEPLOY_USER)@$(DEPLOY_HOST):$(DEPLOY_PATH)/
	rsync --archive --delete documentation/html/ $(DEPLOY_USER)@$(DEPLOY_HOST):$(DEPLOY_PATH)/doc/

clean:
	$(RM) *~ *.o *.d *.so
	$(RM) $(TARGETS)
	$(RM) -rf doc/{html,latex}

docker:
	docker build -t s2ss .
	docker run -itv $(PWD):/s2ss s2ss

# Include auto-generated dependencies
-include $(wildcard *.d)
