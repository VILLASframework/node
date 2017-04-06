etc:

install-etc:
	install -D -t $(DESTDIR)/etc/villas/node etc/*.conf

clean-etc:

.PHONY: etc install-etc clean-etc