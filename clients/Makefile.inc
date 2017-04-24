ASYNCIP_PATH = $(SRCDIR)/clients/opal/udp/models/send_receive

clients: clients-opal
	
clients-opal:
	$(MAKE) -C $(ASYNCIP_PATH) -f Makefile.mk AsyncIP \
		RTLAB_INTEL_COMPILER=0 \
		PROTOCOL=GTNET_SKT \
		OPAL_LIBS="-lSystem -luuid" \
		OPAL_LIBPATH=-L$(SRCDIR)/thirdparty/libopal/ \
		OPAL_INCPATH=-I$(SRCDIR)/thirdparty/libopal/include/opal

clean-clients:
	$(MAKE) -C $(ASYNCIP_PATH) -f Makefile.mk clean

install-clients:
	$(MAKE) -C $(ASYNCIP_PATH) -f Makefile.mk install

.PHONY: clients clean-clients install-clients