tools:

clean-tools:

install-tools:
	install -m 0755 tools/villas.sh $(PREFIX)/bin/villas

.PHONY: tools clean-tools install-tools