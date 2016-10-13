DEPS_CMAKE = libxil libwebsockets criterion jansson
DEPS_AUTOCONF = libnl libconfig libcurl

DEPS = $(DEPS_CMAKE) $(DEPS_AUTOCONF)

.PHONY: $(DEPS)

# Install all dependencies
thirdparty: libxil

# Install & compile autotools based projects
$(DEPS_AUTOCONF): | $(BUILDDIR)/thirdparty/$$@/
	autoreconf -fi $(SRCDIR)/thirdparty/$@
	cd $(BUILDDIR)/thirdparty/$@ && $(SRCDIR)/thirdparty/$@/configure --prefix=$(PREFIX) && make install

# Install & compile CMake based projects
$(DEPS_CMAKE): | $(BUILDDIR)/thirdparty/$$@/
	cmake -DCMAKE_INSTALL_PREFIX:PATH=$(PREFIX) \
	      -H$(SRCDIR)/thirdparty/$@ \
	      -B$(BUILDDIR)/thirdparty/$@
	make  -C$(BUILDDIR)/thirdparty/$@ install

clean-thirdparty:
	for DEP in $(DEPS); do \
	  pushd $$DEP; \
	    git checkout . ; \
	    git clean -dxf $$DEP . ; \
	  popd; \
	done

install-thirdparty: $(addprefix install-,$(DEPS))

clean-thirdparty: $(addprefix clean-,$(DEPS))

.PHONY: $(DEPS) thirdparty clean-thirdparty install-thirdparty