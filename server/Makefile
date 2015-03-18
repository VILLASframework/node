TARGETS = server send random receive test

# Default target: build everything
all: $(TARGETS)

# Common dependencies for all binaries
OBJS = socket.o if.o utils.o msg.o node.o cfg.o tc.o hooks.o list.o path.o hist.o

# Dependencies for individual binaries
server: server.o $(OBJS) 
send: send.o $(OBJS)
receive: receive.o $(OBJS)
random: random.o $(OBJS)
test: test.o $(OBJS)

# Search path for source files
VPATH = src

# Default debug level
V ?= 2

# Compiler and linker flags
LDFLAGS = -pthread -lrt -lm -lconfig
CFLAGS  = -std=c99 -Iinclude/ -MMD -Wall
CFLAGS += -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -DV=$(V)
CFLAGS += -D__GIT_REV__='"-$(shell git rev-parse --short HEAD)"'

# Conditional flags
ifdef DEBUG
	CFLAGS += -O0 -g
else
	CFLAGS += -O3
endif

.PHONY: all clean

clean:
	$(RM) *~ *.o *.d
	$(RM) $(TARGETS)

# Include auto-generated dependencies
-include $(wildcard *.d)
