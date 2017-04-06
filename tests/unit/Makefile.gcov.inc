COVERAGE_TESTS = $(BUILDDIR)/unit-tests
COVERAGE_OBJS = $(LIB_OBJS) $(SRC_OBJS)

GCDAS = $(COVERAGE_OBJS:.o=.gcda)
GCNOS = $(COVERAGE_OBJS:.o=.gcno)

GCOVR_OPTS = --exclude ^include --root $(SRCDIR) --sort-percentage --print-summary

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