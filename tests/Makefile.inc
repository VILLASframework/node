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

-include tests/unit/Makefile.inc
-include tests/integration/Makefile.inc

tests: unit-tests integration-tests

run-tests: run-unit-tests run-integration-tests run-valgrind

VALGRIND = valgrind --leak-check=full --show-leak-kinds=all --suppressions=$(SRCDIR)/tests/valgrind.supp

run-valgrind: src
	$(VALGRIND) $(BUILDDIR)/villas-node & sleep 2; kill %1
	$(VALGRIND) $(BUILDDIR)/villas-pipe -t 1 $(SRCDIR)/etc/websocket-loopback.conf ws1

.PHONY: tests run-tests run-valgrind
