DEPS = libconfig-1.5 libnl-3.2.25 doxygen-1.8.10 pciutils-3.4.0 libwebsockets-master

.PHONY: $(DEPS) all

PREFIX=$(shell pwd)

# Install all dependencies
all: $(DEPS)

# Download latest doxygen
doxygen-1.8.10:
	wget -O- http://ftp.stack.nl/pub/users/dimitri/doxygen-1.8.10.linux.bin.tar.gz | tar xzC $(PREFIX)

# Install & compile libconfig dependency
libconfig-1.5:
	wget -O- http://www.hyperrealm.com/libconfig/libconfig-1.5.tar.gz | tar -xzC $(PREFIX)
	cd $@ && ./configure --prefix=$(PWD)/usr/ --disable-examples && make install

# Install & compile libnl3 dependency
libnl-3.2.25:
	wget -O- http://www.infradead.org/~tgr/libnl/files/libnl-3.2.25.tar.gz | tar -xzC $(PREFIX)
	cd $@ && ./configure --prefix=$(PWD)/usr/ --disable-cli && make install

# Install & compile libpci dependency
pciutils-3.4.0:
	wget -O- ftp://atrey.karlin.mff.cuni.cz/pub/linux/pci/pciutils-3.4.0.tar.gz | tar -xzC $(PREFIX)
	cd $@ && make && make install-lib PREFIX=$(PREFIX)

# Install & compile libwebsockets dependency
# @todo maybe we should use a static ref here..
libwebsockets-master:
	mkdir -p $@/build
	wget -O- https://api.github.com/repos/warmcat/libwebsockets/tarball | tar --strip-components=1 -xzC $(PREFIX)/$@
	cd $@/build && cmake .. && make && make install