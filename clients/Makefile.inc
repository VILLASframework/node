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

CLIENTS =

include $(SRCDIR)/clients/python/Makefile.inc
include $(SRCDIR)/clients/opal/Makefile.inc

clients: $(addprefix clients-,$(filter-out opal,$(CLIENTS)))
clean-clients: $(addprefix clean-clients-,$(filter-out opal,$(CLIENTS)))
install-clients: $(addprefix install-clients-,$(filter-out opal,$(CLIENTS)))

.PHONY: clients clean-clients install-clients
