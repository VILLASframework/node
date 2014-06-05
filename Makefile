TARGETS = server test

OBJS = utils.o msg.o

SRCS = $(wildcard src/*.c)
DEPS = $(SRCS:src/%.c=dep/%.d)

SUBDIRS = doc

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
GDB = gdb -q
MKDIR = mkdir -p

# Pseudotargets
.SECONDARY:
.SECONDEXPANSION:
.PHONY: all clean realclean debug depend $(SUBDIRS)

all: $(addprefix bin/,$(TARGETS))


bin/%: $$(addprefix build/,$$(OBJS))
	$(LD) $(LDFLAGS) $^ -o $@

build/%.o: src/%.c dep/%.d
	$(CC) $(CFLAGS) $< -o $@

dep/%.d: src/%.c
	$(DEP) $(CFLAGS) $< -o $@

debug: bin/server
	$(GDB) -ex "break main" -ex "run $(ARGS)" bin/server

clean:
	$(RM) build/* bin/*

realclean: clean
	@for DIR in $(SUBDIRS); do $(MAKE) -C $$DIR clean; done

depend: $(DEPS)

$(SUBDIRS):
	$(MAKE) -C $@

-include $(DEPS)
