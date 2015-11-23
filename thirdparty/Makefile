.PHONY: dependencies

# Install dependencies
dependencies: libconfig-1.5 libnl-3.2.25 doxygen-1.8.10 pciutils-3.4.0

# Download latest doxygen
doxygen-1.8.10:
	mkdir -p $(PREFIX)
	wget -O- http://ftp.stack.nl/pub/users/dimitri/doxygen-1.8.10.linux.bin.tar.gz | tar xzC $(PREFIX)

# Install & compile libconfig dependency
libconfig-1.5:
	mkdir -p $(PREFIX)/usr/
	wget -O- http://www.hyperrealm.com/libconfig/libconfig-1.5.tar.gz | tar -xzC $(PREFIX)
	cd libconfig-1.5 && ./configure --prefix=$(PWD)/usr/ --disable-examples && make install

# Install & compile libnl3 dependency
libnl-3.2.25:
	mkdir -p $(PREFIX)/usr/
	wget -O- http://www.infradead.org/~tgr/libnl/files/libnl-3.2.25.tar.gz | tar -xzC $(PREFIX)
	cd libnl-3.2.25 && ./configure --prefix=$(PWD)/usr/ --disable-cli && make install

# Install & compile libpci dependency
pciutils-3.4.0:
	mkdir -p $(PREFIX)/usr/
	wget -O- ftp://atrey.karlin.mff.cuni.cz/pub/linux/pci/pciutils-3.4.0.tar.gz | tar -xzC $(PREFIX)
	cd pciutils-3.4.0 && make && make install-lib PREFIX=$(PWD)/usr/