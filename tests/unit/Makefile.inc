# Makefile.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASnode
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
###################################################################################

TEST_SRCS = $(wildcard tests/unit/*.c)
TEST_OBJS = $(patsubst %.c,$(BUILDDIR)/%.o,$(TEST_SRCS))

TEST_CFLAGS = $(CFLAGS)
TEST_LDFLAGS = $(LDFLAGS) -Wl,-rpath,'$$ORIGIN'
TEST_LDLIBS = $(LDLIBS) -lcriterion -lvillas -pthread -ljansson

unit-tests: $(BUILDDIR)/unit-tests

run-unit-tests: tests
	$(BUILDDIR)/unit-tests

# Compile
$(BUILDDIR)/tests/unit/%.o: tests/unit/%.c $(BUILDDIR)/include/villas/config.h | $$(dir $$@)
	$(CC) $(TEST_CFLAGS) -c $< -o $@

# Link
$(BUILDDIR)/unit-tests: $(TEST_OBJS) $(LIB)
	$(CC) $(TEST_LDFLAGS) $^ $(TEST_LDLIBS) -o $@

# Tests are not installed
install-tests:

clean-tests:
	rm -rf $(BUILDDIR)/tests $(BUILDDIR)/testsuite

.PHONY: unit-tests install-unit-tests clean-unit-tests run-unit-tests
