-include tests/unit/Makefile.inc
-include tests/integration/Makefile.inc

tests: unit-tests integration-tests

run-tests: run-unit-tests run-integration-tests

.PHONY: tests run-tests
