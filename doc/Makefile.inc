doc: | $(BUILDDIR)/doc/
	( cat Doxyfile ; echo "OUTPUT_DIRECTORY=$(BUILDDIR)/doc/" ) | doxygen -

install-doc: doc
	mkdir -p $(DESTDIR)$(PREFIX)/share/villas/node/doc/
	cp -R $(BUILDDIR)/doc/html/* $(DESTDIR)$(PREFIX)/share/villas/node/doc/

clean-doc:
	rm -rf $(BUILDDIR)/doc/

.PHONY: doc install-doc clean-doc