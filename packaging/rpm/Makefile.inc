RPMDIR = $(BUILDDIR)/packaging/rpm

SPEC_VILLAS = $(BUILDDIR)/packaging/rpm/villas-node.spec

rpm: rpm-villas-node rpm-libwebsockets rpm-libxil

rpm-villas-node: $(TAR_VILLAS) $(SPEC_VILLAS) | $(RPMDIR)/SOURCES/
	cp $(BUILDDIR)/packaging/villas-node-$(VERSION_NUM).tar.gz $(RPMDIR)/SOURCES
	rpmbuild -ba --define="_topdir $$(pwd)/$(RPMDIR)" $(BUILDDIR)/packaging/rpm/villas-node.spec

rpm-libxil: $(BUILDDIR)/thirdparty/libxil/
	cmake -DCMAKE_INSTALL_PREFIX:PATH=$(PREFIX) \
	      -H$(SRCDIR)/thirdparty/libxil \
	      -B$(BUILDDIR)/thirdparty/libxil $(CMAKE_OPTS)
	make -C$(BUILDDIR)/thirdparty/libxil package_source
	cp $(BUILDDIR)/thirdparty/libxil/libxil-*.tar.gz $(RPMDIR)/SOURCES
	rpmbuild -ba --define="_topdir $$(pwd)/$(RPMDIR)" $(SRCDIR)/thirdparty/libxil/libxil.spec

rpm-libwebsockets: | $(RPMDIR)/RPMS $(BUILDDIR)/thirdparty/libwebsockets/
	cmake -DCMAKE_INSTALL_PREFIX:PATH=$(PREFIX) \
	      -H$(SRCDIR)/thirdparty/libwebsockets \
	      -B$(BUILDDIR)/thirdparty/libwebsockets $(CMAKE_OPTS)
	make  -C$(BUILDDIR)/thirdparty/libwebsockets libwebsockets_rpm
	mv $(BUILDDIR)/thirdparty/libwebsockets/RPM/RPMS/x86_64/libwebsockets-*.rpm $(RPMDIR)/RPMS/x86_64/

# We patch version number and release fields of the spec file based on the current Git commit
$(SPEC_VILLAS): $(SRCDIR)/packaging/rpm/villas-node.spec | $$(dir $$@)
	sed -e "s/§VERSION§/$(VERSION_NUM)/g" \
	    -e "s/§RELEASE§/1.$(GIT_BRANCH)_$(subst -,_,$(VARIANT)).$(shell date +%Y%m%d)git$(GIT_REV)/g" < $^ > $@

clean-rpm:
	rm -rf $(RPMDIR)

.PHONY: rpm clean-rpm rpm-libwebsockets rpm-libxil