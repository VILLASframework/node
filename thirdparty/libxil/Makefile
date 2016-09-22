LIB = libxil.so

PREFIX ?= /usr/local

SRCS  = $(wildcard src/*.c)
SRCS += $(wildcard orig/*/src/*.c)
OBJS  = $(SRCS:c=o)

CC = gcc
CFLAGS = -O3 -fPIC -Iinclude/xilinx -Iorig/common_v1_00_a/src -Wno-int-conversion -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

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
	install -m 0644 include/xilinx/*.h $(PREFIX)/include/xilinx
	ldconfig
