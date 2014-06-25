TARGETS = server send receive random test
SRCS = server.c send.c receive.c random.c node.c path.c utils.c msg.c cfg.c if.c

# Default target: build everything
all: $(TARGETS)

# Dependencies for individual binaries
server: node.o msg.o utils.o path.o cfg.o if.o
send: node.o msg.o utils.o
receive: node.o msg.o utils.o
random: node.o msg.o utils.o
test: node.o msg.o utils.o

VPATH = src

CC = gcc
RM = rm -f
DOXY = doxygen

# Debug level (if not set via 'make V=?')
V ?= 6

# Some details about the compiled version
GIT_TAG = $(shell git describe --tags --abbrev=0)
GIT_REV = $(shell git rev-parse --short HEAD)

# Compiler and Linker flags
LDFLAGS = -pthread -lrt -lm -lconfig
CFLAGS = -g -std=c99 -Iinclude/ -MMD -Wall
CFLAGS += -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -DV=$(V)
CFLAGS += -D__GIT_REV__='"$(GIT_REV)"' -D__GIT_TAG__='"$(GIT_TAG)"'

.PHONY: all clean doc

clean:
	$(RM) *~ *.o *.d
	$(RM) $(TARGETS)

doc:
	$(DOXY)
	$(MAKE) -C doc/latex

# Include auto-generated dependencies
-include $(wildcard *.d)
