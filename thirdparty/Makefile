DEPS = libconfig-1.5 libnl-3.2.25 doxygen-1.8.10 pciutils-3.4.1 libwebsockets-1.7.5

.PHONY: $(DEPS) all

TMPDIR ?= $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
PREFIX ?= /usr/local

# Install all dependencies
all: $(DEPS)

# Download latest doxygen
doxygen-1.8.10:
	wget -O- http://ftp.stack.nl/pub/users/dimitri/doxygen-1.8.10.linux.bin.tar.gz | tar xzC $(TMPDIR)

# Install & compile libconfig dependency
libconfig-1.5:
	wget -O- http://www.hyperrealm.com/libconfig/libconfig-1.5.tar.gz | tar -xzC $(TMPDIR)
	cd $@ && ./configure --prefix=$(PREFIX) --disable-examples && make install

# Install & compile libnl3 dependency
libnl-3.2.25:
	wget -O- http://www.infradead.org/~tgr/libnl/files/libnl-3.2.25.tar.gz | tar -xzC $(TMPDIR)
	cd $@ && ./configure --prefix=$(PREFIX) --disable-cli && make install

# Install & compile libpci dependency
pciutils-3.4.1:
	wget -O- ftp://atrey.karlin.mff.cuni.cz/pub/linux/pci/pciutils-3.4.1.tar.gz | tar -xzC $(TMPDIR)
	cd $@ && make clean && make SHARED=yes && make install-lib PREFIX=$(PREFIX)
	ln -s $(PREFIX)/lib/libpci.so.3.4.1 $(PREFIX)/lib/libpci.so

# Install & compile libwebsockets dependency
libwebsockets-2.0.2:
	mkdir -p $@/build
	wget -O- https://github.com/warmcat/libwebsockets/archive/v2.0.2.tar.gz | tar -xzC $(TMPDIR)
	cd $@/build && cmake -DCMAKE_INSTALL_PREFIX:PATH=$(PREFIX) .. && make install