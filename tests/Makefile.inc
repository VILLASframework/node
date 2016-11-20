TEST_SRCS = $(wildcard tests/*.c)
TEST_OBJS = $(patsubst %.c,$(BUILDDIR)/%.o,$(TEST_SRCS))

TEST_CFLAGS = $(CFLAGS)
TEST_LDFLAGS = $(LDFLAGS) -Wl,-rpath,'$$ORIGIN'
TEST_LDLIBS = $(LDLIBS) -lcriterion -lvillas -pthread

tests: $(BUILDDIR)/testsuite

run-tests: tests
	$(BUILDDIR)/testsuite

# Compile
$(BUILDDIR)/tests/%.o: tests/%.c | $$(dir $$@)
	$(CC) $(TEST_CFLAGS) -c $< -o $@

# Link
$(BUILDDIR)/testsuite: $(TEST_OBJS) | $(LIBS)
	$(CC) $(TEST_LDFLAGS) $(TEST_LDLIBS) $^ -o $@

ifdef COVERAGE
-include tests/Makefile.gcov.inc
endif

# Tests are not installed
install-tests:
	
clean-tests:
	rm -rf $(BUILDDIR)/tests $(BUILDDIR)/testsuite

.PHONY: tests install-tests clean-tests run-tests