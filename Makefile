# This Makefile is mainy used by Travis-CI

PREFIX=$(PWD)/thirdparty

COMMIT=$(shell git rev-parse --short HEAD)

.PHONY: dependencies build deploy test doc clean

clean:
	make -C server clean
	rm -rf thirdparty
	rm -rf documentation/{html,latex}

# Install dependencies
dependencies: $(PREFIX)/libconfig-1.5 $(PREFIX)/libnl-3.2.25 $(PREFIX)/doxygen-1.8.10

# Download latest doxygen
$(PREFIX)/doxygen-1.8.10:
	mkdir -p $(PREFIX)
	wget -O- http://ftp.stack.nl/pub/users/dimitri/doxygen-1.8.10.linux.bin.tar.gz | tar xzC $(PREFIX)

# Install & compile libconfig dependency
$(PREFIX)/libconfig-1.5:
	mkdir -p $(PREFIX)/usr/
	wget -O- http://www.hyperrealm.com/libconfig/libconfig-1.5.tar.gz | tar -xzC $(PREFIX)
	cd $(PREFIX)/libconfig-1.5 && ./configure --prefix=$(PREFIX)/usr/ --disable-examples
	make -C $(PREFIX)/libconfig-1.5 install

# Install & compile libnl3 dependency
$(PREFIX)/libnl-3.2.25:
	mkdir -p $(PREFIX)/usr/
	wget -O- http://www.infradead.org/~tgr/libnl/files/libnl-3.2.25.tar.gz | tar -xzC $(PREFIX)
	cd $(PREFIX)/libnl-3.2.25 && ./configure --prefix=$(PREFIX)/usr/ --disable-cli
	make -C $(PREFIX)/libnl-3.2.25 install

# Compile S2SS server
build: dependencies
	CFLAGS=-I$(PREFIX)/usr/include/ \
	LDFLAGS=-Wl,-L$(PREFIX)/usr/lib/ \
	NLDIR=$(PREFIX)/usr/include/libnl3/ \
	make -C server

# Test S2SS server by running it for 3 secs
test: build
	LD_LIBRARY_PATH=$(PREFIX)/usr/lib/ \
	timeout --signal INT --preserve-status 3s \
	server/server server/etc/loopback.conf || true

# Deploy
deploy: build
	tar czf s2ss-$(COMMIT)-docs.tar.gz documentation/html/
	tar czf s2ss-$(COMMIT).tar.gz server/server server/test server/send server/receive server/random server/etc/
	rsync *.tar.gz $(DEPLOY_USER)@$(DEPLOY_HOST):$(DEPLOY_PATH)/
	rsync --archive --delete documentation/html/ $(DEPLOY_USER)@$(DEPLOY_HOST):$(DEPLOY_PATH)/doc/

# Generate documentation
doc: $(PREFIX)/doxygen-1.8.10
	PATH=$(PREFIX)/doxygen-1.8.10/bin/:$(PATH) \
	doxygen