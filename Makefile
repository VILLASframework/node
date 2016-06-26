# Executables
TARGETS = node pipe signal test

# Libraries
LIBS = libvillas.so thirdparty/xilinx/libxil.so

DEBUG = 1

# Object files for libvillas
LIB_OBJS = sample.o path.o node.o \
	   kernel.o rt.o \
	   list.o pool.o queue.o lstack.o \
	   log.o \
	   utils.o \
	   cfg.o \
	   hooks.o hooks-other.o hooks-internal.o hooks-stats.o \
	   hist.o \
	   timing.o

# Source directories
VPATH = src lib lib/kernel lib/nodes lib/hooks lib/fpga

# Default prefix for installation
PREFIX ?= /usr/local

# Default debug level
V ?= 2

GIT_REV=$(shell git rev-parse --short HEAD)

# Compiler and linker flags
LDLIBS  = -pthread -lrt -lm -lconfig -lvillas -ldl

LIB_LDFLAGS = -shared

CFLAGS += -std=c11 -Iinclude -Iinclude/villas -I. -MMD -mcx16
CFLAGS += -Wall -fdiagnostics-color=auto
CFLAGS += -D_GIT_REV='"$(GIT_REV)"' -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE=1 -DV=$(V)
LDFLAGS += -pthread -L. -Wl,-rpath,'$$ORIGIN'

# pkg-config dependencies
PKGS = libconfig

#DOCKEROPTS = -p 80:80 -p 443:443 --ulimit memlock=1073741824 --security-opt seccomp:unconfined
DOCKEROPTS = -p 1234 --ulimit memlock=1073741824 --security-opt seccomp:unconfined

# Add more compiler flags
ifdef DEBUG
	CFLAGS += -O0 -g
else
	CFLAGS += -O3
endif

######## Node types ########

# file node-type is always supported
LIB_OBJS += file.o cbuilder.o

# Enable Socket node type when libnl3 is available
ifeq ($(shell pkg-config libnl-route-3.0; echo $$?),0)
	LIB_OBJS    += socket.o nl.o tc.o if.o msg.o
	PKGS        += libnl-route-3.0
endif

# Enable VILLASfpga support when libpci is available
ifeq ($(shell pkg-config libpci; echo $$?),0)
	LIB_OBJS    += fpga.o pci.o ip.o vfio.o
	LIB_OBJS    += dma.o model.o fifo.o switch.o rtds_axis.o intc.o dft.o timer.o bram.o
	LDLIBS      += -lxil
	LDFLAGS     += -Lthirdparty/xilinx -Wl,-rpath-link,'$$ORIGIN/thirdparty/xilinx'
	CFLAGS      += -Ithirdparty/xilinx/include
	PKGS        += libpci
	TARGETS     += fpga
endif

# Enable NGSI support
ifeq ($(shell pkg-config libcurl jansson uuid; echo $$?),0)
	LIB_OBJS    += ngsi.o
	PKGS        += libcurl jansson uuid
endif

## Enable WebSocket support
#ifeq ($(shell pkg-config libwebsockets jansson; echo $$?),0)
#	LIB_OBJS   += websocket.o websocket-live.o websocket-http.o
#	PKGS       += libwebsockets jansson
#endif

# Enable OPAL-RT Asynchronous Process support (will result in 32bit binary!!!)
ifdef WITH_OPAL
ifneq (,$(wildcard thirdparty/opal/include/AsyncApi.h))
	LIB_OBJS    += opal.o
	CFLAGS      += -m32
	LDFLAGS     += -m32
	LIB_CFLAGS  += -m32 -I thirdparty/opal/include
	LIB_LDFLAGS += -m32 -L/lib/i386-linux-gnu/ -L/usr/lib/i386-linux-gnu/ -Lthirdparty/opal/lib/redhawk/
	LIB_LDLIBS  += -lOpalAsyncApiCore -lOpalCore -lOpalUtils -lirc
endif
endif

# Add flags by pkg-config
LIB_CFLAGS += $(addprefix -DWITH_, $(shell echo ${PKGS} | tr a-z- A-Z_ | tr -dc ' A-Z0-9_' ))
LIB_CFLAGS += $(shell pkg-config --cflags ${PKGS})
LIB_LDLIBS += $(shell pkg-config --libs ${PKGS})

######## Targets ########

.PHONY: all clean install release docker doc models

# Default target: build everything
all: $(LIBS) $(TARGETS) models

# Dependencies for individual binaries
fpga: LDLIBS += -lpci -lxil

node:   server.o
fpga:   fpga-main.o fpga-tests.o fpga-bench.o
pipe:   pipe.o
test:   test.o
signal: signal.o

# Libraries
$(LIB_OBJS): CFLAGS += -fPIC $(LIB_CFLAGS)
libvillas.so: $(LIB_OBJS)
	$(CC) $(LIB_LDFLAGS) -o $@ $^ $(LIB_LDLIBS)

thirdparty/xilinx/libxil.so:
	$(MAKE) -C thirdparty/xilinx libxil.so

# Common targets
install: $(TARGETS) $(LIBS)
	install -m 0644 $(LIBS) $(PREFIX)/lib
	install -m 0755 node -T $(PREFIX)/bin/villas-node
	install -m 0755 fpga $(PREFIX)/bin/villas-fpga
	install -m 0755 signal $(PREFIX)/bin/villas-signal
	install -m 0755 pipe $(PREFIX)/bin/villas-pipe
	install -m 0755 test $(PREFIX)/bin/villas-test
	install -m 0755 tools/villas.sh $(PREFIX)/bin/villas
	install -m 0755 -d $(PREFIX)/include/villas/
	install -m 0644 include/*.h $(PREFIX)/include/villas/
	ldconfig

release: all
	tar czf villas-$(COMMIT)-doc.tar.gz doc/html/
	tar czf villas-$(COMMIT).tar.gz node test pipe signal etc/
	rsync *.tar.gz $(DEPLOY_USER)@$(DEPLOY_HOST):$(DEPLOY_PATH)/
	rsync --archive --delete doc/html/ $(DEPLOY_USER)@$(DEPLOY_HOST):$(DEPLOY_PATH)/doc/

clean:
	$(RM) *~ *.o *.d *.so
	$(RM) $(TARGETS)
	$(RM) -rf doc/{html,latex}

docker:
	docker build -t villas .
	docker run -it $(DOCKEROPTS) -v $(PWD):/villas villas

doc:
	doxygen

models:
	$(MAKE) -C lib/cbmodels

# Include auto-generated dependencies
-include $(wildcard *.d)
