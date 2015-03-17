TARGETS = server send random receive test
SRCS = server.c send.c receive.c random.c node.c path.c utils.c socket.c msg.c cfg.c if.c tc.c hist.c

COMMON = socket.o if.o utils.o msg.o node.o cfg.o tc.o hooks.o

VPATH = src

# Default debug level
V ?= 2

# Some details about the compiled version

# Compiler and linker flags
LDLIBS = -pthread -lrt -lm -lconfig
CFLAGS  = -std=c99 -Iinclude/ -MMD -Wall
CFLAGS += -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -DV=$(V)
CFLAGS += -D__GIT_REV__='"-$(shell git rev-parse --short HEAD)"'

# Conditional flags
ifdef DEBUG
	CFLAGS += -g
else
	CFLAGS += -O3
endif

# Enable OPAL-RT Asynchronous Process support
#OPALDIR = /usr/opalrt/common
OPALDIR = ../opal
ifneq (,$(wildcard $(OPALDIR)/include_target/AsyncApi.h))
	CFLAGS  += -m32 -DENABLE_OPAL_ASYNC -I$(OPALDIR)/include_target
	LDFLAGS += -m32
	LDLIBS  += $(addprefix $(OPALDIR)/lib/redhawk/, libOpalAsyncApiCore.a libOpalCore.a libOpalUtils.a libirc.a)
	COMMON  += opal.o
endif

.PHONY: all clean

# Default target: build everything
all: $(TARGETS)

# Specific dependencies
server:  $(COMMON) server.o path.o hist.o
send:    $(COMMON) send.o
receive: $(COMMON) receive.o
random: random.o utils.o msg.o
test:    $(COMMON) hist.o

clean:
	$(RM) *~ *.o *.d
	$(RM) $(TARGETS)

# Include auto-generated dependencies
-include $(wildcard *.d)
