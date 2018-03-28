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

COVERAGE_TESTS = $(BUILDDIR)/unit-tests
COVERAGE_OBJS = $(LIB_OBJS) $(SRC_OBJS)

GCDAS = $(COVERAGE_OBJS:.o=.gcda)
GCNOS = $(COVERAGE_OBJS:.o=.gcno)

GCOVR_OPTS = --exclude $(SRCDIR)/include --root $(SRCDIR) --sort-percentage --print-summary

coverage: $(BUILDDIR)/coverage/index.html $(BUILDDIR)/coverage.xml $(BUILDDIR)/coverage.txt

$(BUILDDIR)/coverage.txt:  $(addsuffix .gcdas,$(COVERAGE_TESTS)) | $$(dir $$@)
	gcovr $(GCOVR_OPTS) -o $@

$(BUILDDIR)/coverage.xml:  $(addsuffix .gcdas,$(COVERAGE_TESTS)) | $$(dir $$@)
	gcovr $(GCOVR_OPTS) --xml --xml-pretty -o $@

$(BUILDDIR)/coverage/index.html: $(addsuffix .gcdas,$(COVERAGE_TESTS)) | $$(dir $$@)
	gcovr $(GCOVR_OPTS) --html --html-details -o $@

# This is an intermediate target. It is used to run the test only once for all gcovr rules.
%.gcdas: %
	@echo "Delete previous coverage information"
	rm -f $(GCDAS)
	@echo "Run $< for collecting coverage information (.gcda)"
	$^

clean-coverage:
	rm -rf $(BUILDDIR)/coverage $(BUILDDIR)/coverage.txt $(BUILDDIR)/coverage.xml
	rm -f $(GCDAS)
	
install-coverage:

.INTERMEDIATE: $(addsuffix .gcdas,$(COVERAGE_TESTS))
.PHONY: coverage gcda clean-coverage install-coverage
