LIB = libxil.so

PREFIX ?= /usr/local

SRCDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

VPATH = $(SRCDIR)

SRCS  = $(wildcard $(SRCDIR)/src/*.c)
SRCS += $(wildcard $(SRCDIR)/orig/*/src/*.c)
OBJS  = $(SRCS:$(SRCDIR)/%.c=%.o)

CC = gcc
CFLAGS = -O3 -fPIC -I$(SRCDIR)/include/xilinx -Iorig/common_v1_00_a/src -Wno-int-conversion -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

ifdef DEBUG
	CFLAGS += -DDEBUG -DXDEBUG_WARNING -g
endif

all: $(LIB)

$(LIB): $(OBJS)
	$(CC) -shared -o $@ $^

clean:
	rm -f $(LIB) $(OBJS)

install: $(LIB)
	mkdir -p $(PREFIX)/include/xilinx
	install -m 0644 $(LIB) $(PREFIX)/lib
	install -m 0644 $(SRCDIR)/include/xilinx/*.h $(PREFIX)/include/xilinx
	@echo "note: you may need to run 'ldconfig'"
	@echo "note: make sure $(PREFIX)/lib is in your /etc/ld.so.conf or $$LD_LIBRARY_PATH"

.SECONDEXPANSION:
	
$(OBJS): | $$(dir $$@)

%/:
	mkdir -p $@