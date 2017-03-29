TEST_SRCS = $(wildcard tests/unit/*.c)
TEST_OBJS = $(patsubst %.c,$(BUILDDIR)/%.o,$(TEST_SRCS))

TEST_CFLAGS = $(CFLAGS)
TEST_LDFLAGS = $(LDFLAGS) -Wl,-rpath,'$$ORIGIN'
TEST_LDLIBS = $(LDLIBS) -lcriterion -lvillas -pthread -ljansson

unit-tests: $(BUILDDIR)/unit-tests

run-unit-tests: tests
	$(BUILDDIR)/unit-tests

# Compile
$(BUILDDIR)/tests/unit/%.o: tests/unit/%.c | $$(dir $$@)
	$(CC) $(TEST_CFLAGS) -c $< -o $@

# Link
$(BUILDDIR)/unit-tests: $(TEST_OBJS) | $(LIBS)
	$(CC) $(TEST_LDFLAGS) $(TEST_LDLIBS) $^ -o $@

ifdef COVERAGE
-include tests/Makefile.gcov.inc
endif

# Tests are not installed
install-tests:
	
clean-tests:
	rm -rf $(BUILDDIR)/tests $(BUILDDIR)/testsuite

.PHONY: unit-tests install-unit-tests clean-unit-tests run-unit-tests