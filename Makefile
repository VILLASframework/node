TARGETS = server test

OBJS = utils.o msg.o

SRCS = $(wildcard src/*.c)
DEPS = $(SRCS:src/%.c=dep/%.d)

DIRS = dep bin build

# Files
bin/server: OBJS += main.o node.o path.o
bin/test: OBJS += test.o

# Flags
INCS = -Iinclude/
DEFS = -D_XOPEN_SOURCE -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
LIBS = -lrt

CFLAGS = -g -std=c99 $(DEFS) $(INCS)
LDFLAGS = -pthread $(LIBS)

# Tools
RM = rm -f
CC = gcc -c
LD = gcc
DEP = gcc -MM
MKDIR = mkdir -p
RMDIR = rm -rf

# Pseudotargets
.SECONDARY:
.SECONDEXPANSION:
.PHONY: all clean doc

all:    $(addprefix bin/,$(TARGETS))

bin/%: $$(addprefix build/,$$(OBJS)) | bin
	$(LD) $(LDFLAGS) $^ -o $@

build/%.o: src/%.c dep/%.d | build
	$(CC) $(CFLAGS) $< -o $@

dep/%.d: src/%.c | dep
	$(DEP) $(CFLAGS) $< -o $@

clean:
	$(RMDIR) $(DIRS)

$(DIRS):
	$(MKDIR) $@

doc:
	$(MAKE) -C $@

-include $(DEPS)
