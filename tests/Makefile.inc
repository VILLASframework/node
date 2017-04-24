-include tests/unit/Makefile.inc
-include tests/integration/Makefile.inc

tests: unit-tests integration-tests

run-tests: run-unit-tests run-integration-tests run-valgrind

run-valgrind: src
	valgrind --leak-check=full --show-leak-kinds=all --suppressions=$(SRCDIR)/tests/valgrind.supp $(BUILDDIR)/villas-node & sleep 2; kill $$!; sleep 1; kill $$!
	valgrind --leak-check=full --show-leak-kinds=all --suppressions=$(SRCDIR)/tests/valgrind.supp $(BUILDDIR)/villas-pipe $(SRCDIR)/etc/websocket-loopback.conf ws1 & sleep 2; kill $$!; sleep 1; kill $$!

.PHONY: tests run-tests run-valgrind
