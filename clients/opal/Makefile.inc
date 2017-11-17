# Makefile.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASnode
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
###################################################################################

ifneq ($(wildcard $(SRCDIR)/thirdparty/libopal/include/opal/AsyncApi.h),)

CLIENTS += opal

ASYNCIP_PATH = $(SRCDIR)/clients/opal/models/send_receive

ASYNCIP_OPTS = RTLAB_INTEL_COMPILER=0 \
		PROTOCOL=GTNET_SKT \
		OPAL_LIBS="-lSystem -luuid" \
		OPAL_LIBPATH=-L$(SRCDIR)/thirdparty/libopal/ \
		OPAL_INCPATH=-I$(SRCDIR)/thirdparty/libopal/include/opal
		
clients-opal:
	$(MAKE) -C $(ASYNCIP_PATH) -f Makefile.mk AsyncIP $(ASYNCIP_OPTS)
		
clean-clients-opal:
	$(MAKE) -C $(ASYNCIP_PATH) -f Makefile.mk clean $(ASYNCIP_OPTS)

install-clients-opal:
	$(MAKE) -C $(ASYNCIP_PATH) -f Makefile.mk install $(ASYNCIP_OPTS)

.PHONY: clients-opal clean-clients-opal install-clients-opal

endif
