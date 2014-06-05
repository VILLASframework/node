TARGETS = server send receive random test

SRCS = server.c send.c receive.c random.c node.c path.c utils.c msg.c cfg.c caps.c
VPATH = src

CC = gcc
RM = rm -f

GIT_TAG = $(shell git describe --tags  --abbrev=0)
GIT_REV = $(shell git rev-parse --short HEAD)

V ?= 4

LDFLAGS = -pthread -lrt -lm -lconfig -lnl-3 -lnl-route-3
CFLAGS = -g -std=c99 -Iinclude/ -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -DV=$(V)
CFLAGS += -D__GIT_REV__='"$(GIT_REV)"' -D__GIT_TAG__='"$(GIT_TAG)"'

.PHONY: all clean doc

all: $(TARGETS)

server: node.o msg.o utils.o path.o cfg.o caps.o
send: node.o msg.o utils.o
receive: node.o msg.o utils.o
random: node.o msg.o utils.o
test: node.o msg.o utils.o

%.d: %.c
	$(CC) -MM $(CFLAGS) $< -o $@

clean:
	$(RM) *~ *.o *.d
	$(RM) $(TARGETS)

doc:
	$(MAKE) -C $@

-include $(SRCS:.c=.d)
