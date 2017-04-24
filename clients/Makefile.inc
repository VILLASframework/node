ASYNCIP_PATH = $(SRCDIR)/clients/opal/villas_udp/models/send_receive

ASYNCIP_OPTS = RTLAB_INTEL_COMPILER=0 \
		PROTOCOL=GTNET_SKT \
		OPAL_LIBS="-lSystem -luuid" \
		OPAL_LIBPATH=-L$(SRCDIR)/thirdparty/libopal/ \
		OPAL_INCPATH=-I$(SRCDIR)/thirdparty/libopal/include/opal

clients: clients-opal
	
clients-opal:
	$(MAKE) -C $(ASYNCIP_PATH) -f Makefile.mk AsyncIP $(ASYNCIP_OPTS)

clean-clients:
	$(MAKE) -C $(ASYNCIP_PATH) -f Makefile.mk clean $(ASYNCIP_OPTS)

install-clients:
	$(MAKE) -C $(ASYNCIP_PATH) -f Makefile.mk install $(ASYNCIP_OPTS)

.PHONY: clients clean-clients install-clients