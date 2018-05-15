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

TAR_OPTS = --exclude-ignore-recursive=$(SRCDIR)/.distignore --transform='s|^\.|villas-node-$(VERSION_NUM)|' --show-transformed-names

TAR_VILLAS = $(BUILDDIR)/packaging/villas-node-$(VERSION_NUM)-$(RELEASE).tar.gz

DEPLOY_USER ?= deploy
DEPLOY_HOST ?= acs-os-fein-website
DEPLOY_PATH ?= /var/www/villas/node

packaging: rpm dist
	
deploy: deploy-dist deploy-rpm deploy-docker
	
# Source tarballs
dist: $(TAR_VILLAS)

$(TAR_VILLAS): | $$(dir $$@)
	tar $(TAR_OPTS) -C $(SRCDIR) -czf $@ .

deploy-dist: $(TAR_VILLAS)
	rsync $(TAR_VILLAS) $(DEPLOY_USER)@$(DEPLOY_HOST):$(DEPLOY_PATH)/src
	
deploy-rpm:
	rsync -a --progress $(BUILDDIR)/../Linux-x86_64-release/packaging/rpm/RPMS/ $(DEPLOY_USER)@$(DEPLOY_HOST):/var/www/packages/redhat/
	ssh $(DEPLOY_USER)@$(DEPLOY_HOST) createrepo /var/www/packages/redhat

clean-packaging:
	rm -f $(BUILDDIR)/packaging/villas-node-$(VERSION_NUM).tar.gz

install-packaging:

.PHONY: packaging install-packaging clean-packaging deploy deploy-dist deploy-rpm dist $(TAR_VILLAS)

include packaging/rpm/Makefile.inc
include packaging/docker/Makefile.inc
