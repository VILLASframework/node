TARGETS += $(BUILDDIR)/tests

TEST_SRCS = $(wildcard tests/*.c)
TEST_OBJS = $(patsubst %.c,$(BUILDDIR)/%.o,$(TEST_SRCS))

TEST_CFLAGS = $(CFLAGS)
TEST_LDFLAGS = $(LDFLAGS)
TEST_LDLIBS = $(LDLIBS) -l criterion

# Compile
$(BUILDDIR)/tests/%.o: tests/%.c | $$(dir $$@)
	$(CC) $(TEST_CFLAGS) -c $< -o $@

# Link
$(BUILDDIR)/tests: $(TEST_OBJS) | $(LIBS)
	$(CC) $(TEST_LDFLAGS) $(TEST_LDLIBS) $^ -o $@#

ifdef COVERAGE
-include Makefile.gcov.inc
endif