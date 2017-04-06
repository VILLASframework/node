install-web:
	cp -R web $(DESTDIR)$(PREFIX)/share/villas/node

clean-web:

.PHONY: web install-web clean-web