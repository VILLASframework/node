LIB_NAME = libvillas
LIB_ABI_VERSION = 1

LIB = $(BUILDDIR)/$(LIB_NAME).so.$(LIB_ABI_VERSION)

# Object files for libvillas
LIB_SRCS = $(addprefix lib/nodes/, file.c cbuilder.c shmem.c) 		\
           $(addprefix lib/kernel/, kernel.c rt.c)		\
           $(addprefix lib/, sample.c path.c node.c hook.c	\
              log.c utils.c super_node.c hist.c timing.c pool.c	\
              list.c queue.c queue_signalled.c memory.c advio.c web.c api.c	\
              plugin.c node_type.c stats.c mapping.c sample_io.c\
           )

LIB_CFLAGS  = $(CFLAGS) -fPIC
LIB_LDFLAGS = -shared
LIB_LDLIBS  = $(LDLIBS) -ldl -lrt -Wl,-soname,$(LIB_NAME).so.$(LIB_ABI_VERSION)

-include lib/hooks/Makefile.inc
-include lib/apis/Makefile.inc
-include lib/fpga/Makefile.inc

LIB_PKGS = openssl

######## Node types ########

# Enable Socket node type when libnl3 is available
ifndef WITHOUT_SOCKET
ifeq ($(shell pkg-config libnl-route-3.0; echo $$?),0)
	LIB_SRCS    += $(addprefix lib/nodes/, socket.c)
	LIB_SRCS    += $(addprefix lib/kernel/, nl.c tc.c if.c)
	LIB_SRCS    += $(addprefix lib/, msg.c)
	LIB_PKGS    += libnl-route-3.0
endif
endif

# Enable VILLASfpga support when libxil is available
ifndef WITHOUT_FPGA
ifeq ($(shell pkg-config libxil; echo $$?),0)
	LIB_SRCS    += $(addprefix lib/nodes/, fpga.c)
	LIB_SRCS    += $(addprefix lib/kernel/, pci.c vfio.c)
	PKGS        += libxil
endif
endif

# Enable NGSI support
ifndef WITHOUT_NGSI
ifeq ($(shell pkg-config libcurl jansson; echo $$?),0)
	LIB_SRCS    += lib/nodes/ngsi.c
	LIB_PKGS    += libcurl jansson
endif
endif

# Enable WebSocket support
ifndef WITHOUT_WEBSOCKETS
ifeq ($(shell pkg-config libwebsockets jansson; echo $$?),0)
	LIB_SRCS    += lib/nodes/websocket.c
	LIB_PKGS    += libwebsockets jansson
endif
endif

# Enable OPAL-RT Asynchronous Process support (will result in 32bit binary!!!)
ifdef WITH_OPAL
ifneq (,$(wildcard thirdparty/opal/include/AsyncApi.h))
	LIB_OBJS    += opal.o

	LIB_CFLAGS  += -I thirdparty/opal/include
	LIB_LDFLAGS += -L/lib/i386-linux-gnu/ -L/usr/lib/i386-linux-gnu/ -Lthirdparty/opal/lib/redhawk/
	LIB_LDLIBS  += -lOpalAsyncApiCore -lOpalCore -lOpalUtils -lirc
	
	# libOpalAsyncApi is a 32bit library. So we need to build everything in 32bit
	CFLAGS      += -m32
	LDFLAGS     += -m32
	BUILDDIR    := $(BUILDDIR)32
endif
endif

# Add flags by pkg-config
LIB_CFLAGS += $(shell pkg-config --cflags ${LIB_PKGS})
LIB_LDLIBS += $(shell pkg-config --libs ${LIB_PKGS})

LIB_OBJS = $(patsubst %.c, $(BUILDDIR)/%.o, $(LIB_SRCS))

lib: $(LIB)

# Link
$(LIB): $(LIB_OBJS)
	$(CC) $(LIB_LDFLAGS) -o $@ $^ $(LIB_LDLIBS)
	ln -srf $@ $(BUILDDIR)/$(LIB_NAME).so

# Compile
$(BUILDDIR)/lib/%.o: lib/%.c $(BUILDDIR)/defines | $$(dir $$@)
	$(CC) $(LIB_CFLAGS) -c $< -o $@

# Install
install-lib: lib
	install -m 0755 -D -T $(LIB) $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME).so.$(LIB_ABI_VERSION)
	install -m 0644 -D -t $(DESTDIR)$(PREFIX)/include/villas/ include/villas/*.h
	ln -srf $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME).so.$(LIB_ABI_VERSION) $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME).so
	ldconfig

clean-lib:
	rm -rf $(BUILDDIR)/lib $(LIB)

.PHONY: lib install-lib clean-lib $(BUILDDIR)/lib/defines
