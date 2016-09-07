# Executables
TARGETS = node pipe signal test

# Libraries
LIBS = libvillas.so

# Plugins
PLUGINS = simple_circuit.so example_hook.so

# Object files for libvillas
LIB_SRCS = $(wildcard lib/hooks/*.c)			\
           $(addprefix lib/kernel/, kernel.c rt.c)	\
	   $(addprefix lib/,				\
              sample.c path.c node.c hooks.c		\
              log.c utils.c cfg.c hist.c timing.c	\
              pool.c list.c queue.c lstack.c		\
           )						\

# Default prefix for install target
PREFIX ?= /usr/local

# Default debug level
V ?= 2

# Compiler and linker flags
LDLIBS   = -pthread -lm -lvillas

PLUGIN_CFLAGS = -fPIC -DVILLAS -I../include/villas

LIB_CFLAGS  = -fPIC
LIB_LDFLAGS = -shared
LIB_LDLIBS  = -ldl -lrt

CFLAGS  += -std=c11 -Iinclude -Iinclude/villas -I. -MMD -mcx16
CFLAGS  += -Wall -fdiagnostics-color=auto
CFLAGS  += -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE=1 -DV=$(V)
LDFLAGS += -pthread -L. -Wl,-rpath,'$$ORIGIN'

ifdef CI
	CFLAGS += -D_GIT_REV='"${CI_BUILD_REF:0:7}~ci"'
else ifdef GIT
	CFLAGS += -D_GIT_REV='"$(shell git rev-parse --short HEAD)"'
endif

# pkg-config dependencies
PKGS = libconfig

DOCKEROPTS = -p 80:80 -p 443:443 -p 1234:1234 --privileged --cap-add sys_nic --ulimit memlock=1073741824 --security-opt seccomp:unconfined

# Add more compiler flags
ifdef DEBUG
	CFLAGS += -O0 -g
else
	CFLAGS += -O3
endif

######## Node types ########

# file node-type is always supported
LIB_SRCS += $(addprefix lib/nodes/, file.c cbuilder.c)

# Enable Socket node type when libnl3 is available
ifeq ($(shell pkg-config libnl-route-3.0; echo $$?),0)
	LIB_SRCS    += $(addprefix lib/nodes/, socket.c)
	LIB_SRCS    += $(addprefix lib/kernel/, nl.c tc.c if.c)
	LIB_SRCS    += $(addprefix lib/, msg.c)
	PKGS        += libnl-route-3.0
endif

# Enable VILLASfpga support when libpci is available
ifeq ($(shell pkg-config libpci; echo $$?),0)
	LIB_SRCS    += $(addprefix lib/nodes/, fpga.c)
	LIB_SRCS    += $(addprefix lib/kernel/, pci.c vfio.c)
	LIB_SRCS    += $(wildcard  lib/fpga/*.c)
	LDLIBS      += -lxil
	LDFLAGS     += -Lthirdparty/xilinx -Wl,-rpath,'$$ORIGIN/thirdparty/xilinx'
	CFLAGS      += -Ithirdparty/xilinx/include
	PKGS        += libpci
	TARGETS     += fpga
endif

# Enable NGSI support
ifeq ($(shell pkg-config libcurl jansson uuid; echo $$?),0)
	LIB_SRCS    += lib/nodes/ngsi.c
	PKGS        += libcurl jansson uuid
endif

# Enable WebSocket support
ifeq ($(shell pkg-config libwebsockets jansson; echo $$?),0)
	LIB_SRCS   += lib/nodes/websocket.c
	PKGS       += libwebsockets jansson
endif

## Add support for LAPACK / BLAS benchmarks / solvers
ifeq ($(shell pkg-config blas lapack; echo $$?),0)
	PKGS        += blas lapack
	BENCH_OBJS  += fpga-bench-overruns.o
endif

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

LIB_OBJS = $(patsubst lib/%.c, obj/lib/%.o, $(LIB_SRCS))

######## Targets ########

.PHONY: all clean install docker doc
.SECONDARY:
.SECONDEXPANSION:

# Default target: build everything
all: $(LIBS) $(TARGETS) $(PLUGINS)

# Dependencies for individual binaries
fpga:   $(addprefix obj/src/,fpga.o fpga-tests.o fpga-bench.o $(BENCH_OBJS))
node:   $(addprefix obj/src/,node.o)
pipe:   $(addprefix obj/src/,pipe.o)
test:   $(addprefix obj/src/,test.o)
signal: $(addprefix obj/src/,signal.o)

# Dependencies for plugins
example_hook.so:   obj/plugins/hooks/example_hook.o
simple_circuit.so: obj/plugins/models/simple_circuit.o

libvillas.so: $(LIB_OBJS)

# Create directories
%/:
	mkdir -p $@

# Compile executable objects
obj/src/%.o: src/%.c | $$(dir $$@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile library objects
obj/lib/%.o: lib/%.c | $$(dir $$@)
	$(CC) $(CFLAGS) $(LIB_CFLAGS) -c $< -o $@
	
obj/plugins/%.o: plugins/%.c | $$(dir $$@)
	$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) -c $< -o $@

# Link target executables
$(TARGETS):
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
	
# Link Libraries & Plugins
$(LIBS) $(PLUGINS):
	$(CC) $(LIB_LDFLAGS) -o $@ $^ $(LIB_LDLIBS)

# Common targets
install: $(TARGETS) $(LIBS)
	install -m 0644 $(LIBS) $(PREFIX)/lib
	install -m 0755 node	-T $(PREFIX)/bin/villas-node
	install -m 0755 fpga	-T $(PREFIX)/bin/villas-fpga
	install -m 0755 signal	-T $(PREFIX)/bin/villas-signal
	install -m 0755 pipe	-T $(PREFIX)/bin/villas-pipe
	install -m 0755 test	-T $(PREFIX)/bin/villas-test
	install -m 0755		-d $(PREFIX)/include/villas/
	install -m 0644 include/villas/*.h $(PREFIX)/include/villas/
	install -m 0755 tools/villas.sh $(PREFIX)/bin/villas
	ldconfig

clean:
	$(RM) $(LIBS) $(PLUGINS) $(TARGETS)
	$(RM) -rf obj/ doc/{html,latex}

docker:
	docker build -t villas .
	docker run -it $(DOCKEROPTS) -v $(PWD):/villas villas

doc:
	doxygen

# Include auto-generated dependencies
-include $(wildcard obj/**/*.d)