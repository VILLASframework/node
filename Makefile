TARGETS = server sim random

SRCS = server.c test.c node.c path.c utils.c msg.c
VPATH = src

CC = gcc
RM = rm -f

GIT_TAG = $(shell git describe --tags  --abbrev=0)
GIT_REV = $(shell git rev-parse --short HEAD)

V ?= 4

LDFLAGS = -pthread -lrt -lm
CFLAGS = -g -std=c99 -Iinclude/ -D_XOPEN_SOURCE=500 -DV=$(V)
CFLAGS += -D__GIT_REV__='"$(GIT_REV)"' -D__GIT_TAG__='"$(GIT_TAG)"'

.PHONY: all clean doc

all: $(TARGETS)

server: node.o msg.o utils.o path.o
sim: msg.o utils.o
random: msg.o utils.o

%.d: %.c
	$(CC) -MM $(CFLAGS) $< -o $@

clean:
	$(RM) *~ *.o *.d
	$(RM) $(TARGETS)

doc:
	$(MAKE) -C $@

-include $(SRCS:.c=.d)
