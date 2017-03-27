-include tests/unit/Makefile.inc

tests: unit-tests

run-tests: run-unit-tests run-integration-tests
	
run-integration-tests: src tools
	@$(SRCDIR)/tests/integration-tests.sh

.PHONY: tests run-tests run-integration-tests
