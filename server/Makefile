TARGETS = server send receive random test
SRCS = server.c send.c receive.c random.c node.c path.c utils.c msg.c cfg.c if.c tc.c

# Default target: build everything
all: $(TARGETS)

# Dependencies for individual binaries
server: node.o msg.o utils.o path.o cfg.o if.o tc.o hooks.o
send: node.o msg.o utils.o
receive: node.o msg.o utils.o
random: node.o msg.o utils.o
test: node.o msg.o utils.o

VPATH = src

# Default debug level
V ?= 2

# Some details about the compiled version

# Compiler and linker flags
LDFLAGS = -pthread -lrt -lm -lconfig
CFLAGS  = -std=c99 -Iinclude/ -MMD -Wall
CFLAGS += -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -DV=$(V)
CFLAGS += -D__GIT_REV__='"-$(shell git rev-parse --short HEAD)"'

# Conditional flags
ifdef DEBUG
	CFLAGS += -g
else
	CFLAGS += -O3
endif

.PHONY: all clean

clean:
	$(RM) *~ *.o *.d
	$(RM) $(TARGETS)

# Include auto-generated dependencies
-include $(wildcard *.d)
