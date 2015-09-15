# This Makefile is mainy used by Travis-CI

PREFIX=$(PWD)/thirdparty

COMMIT=$(shell git rev-parse --short HEAD)

.PHONY: dependencies build deploy test doc clean libnl3 libconfig

clean:
	make -C server clean
	rm -rf thirdparty
	rm -rf documentation/{html,latex}
doc:
	doxygen

dirs:
	mkdir -p $(PREFIX)/usr/	

# Install dependencies
dependencies: libconfig libnl3

# Install & compile libconfig dependency
libconfig: dirs
	wget -O- http://www.hyperrealm.com/libconfig/libconfig-1.5.tar.gz | tar -xzC $(PREFIX)
	cd $(PREFIX)/libconfig-1.5 && ./configure --prefix=$(PREFIX)/usr/ --disable-examples
	make -C $(PREFIX)/libconfig-1.5 install

# Install & compile libnl3 dependency
libnl3: dirs
	wget -O- http://www.infradead.org/~tgr/libnl/files/libnl-3.2.25.tar.gz | tar -xzC $(PREFIX)
	cd $(PREFIX)/libnl-3.2.25 && ./configure --prefix=$(PREFIX)/usr/ --disable-cli
	make -C $(PREFIX)/libnl-3.2.25 install

# Compile S2SS server
build:
	CFLAGS=-I$(PREFIX)/usr/include/ \
	LDFLAGS=-Wl,-L$(PREFIX)/usr/lib/ \
	NLDIR=$(PREFIX)/usr/include/libnl3/ \
	make -C server

# Test S2SS server by running it for 3 secs
test:
	LD_LIBRARY_PATH=$(PREFIX)/usr/lib/ \
	timeout --signal INT --preserve-status 3s \
	server/server server/etc/loopback.conf

# Deploy
deploy:
	tar czf s2ss-$(COMMIT)-docs.tar.gz documentation/html/
	tar czf s2ss-$(COMMIT).tar.gz server/server server/test server/send server/receive server/random server/etc/
	rsync *.tar.gz $(DEPLOY_USER)@$(DEPLOY_HOST):$(DEPLOY_PATH)/
	rsync --archive --delete documentation/html/ $(DEPLOY_USER)@$(DEPLOY_HOST):$(DEPLOY_PATH)/doc/