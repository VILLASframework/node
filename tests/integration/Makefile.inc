integration-tests: src tools

run-integration-tests: integration-tests
	@$(SRCDIR)/tests/integration-tests.sh