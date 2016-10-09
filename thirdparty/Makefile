DEPS = libxil libconfig libnl libwebsockets criterion

.PHONY: $(DEPS) all clean

TMPDIR ?= $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
PREFIX ?= /usr/local

# Install all dependencies
all: $(DEPS)

# Install & compile libconfig dependency
libconfig:
	cd $@ && \
	rm -f lib/scanner.[ch] && \
	autoreconf && \
	./configure --prefix=$(PREFIX) --disable-examples && \
	make install

# Install & compile libnl3 dependency
libnl:
	cd $@ && \
	./autogen.sh && \
	./configure --prefix=$(PREFIX) --disable-cli && \
	make install

# Install & compile libwebsockets dependency
libwebsockets:
	mkdir $@/build && cd $@/build && \
	cmake -DCMAKE_INSTALL_PREFIX:PATH=$(PREFIX) .. && \
	make install

# Install & compile Criterion unittest framework
criterion:
	mkdir $@/build && cd $@/build && \
	cmake -DCMAKE_INSTALL_PREFIX:PATH=$(PREFIX) .. && \
	make install

# Install & compile Xilinx standalone drivers
libxil:
	cd $@ && \
	make install

clean:
	for DEP in ${DEPS}; do \
	  pushd $$DEP; \
	    git checkout . ; \
	    git clean -dxf $$DEP . ; \
	  popd; \
	done
