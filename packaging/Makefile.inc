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

TAROPTS = --exclude-ignore-recursive=.distignore --transform='s|^\.|villas-node-$(VERSION_NUM)|' --show-transformed-names

TAR_VILLAS = $(BUILDDIR)/packaging/villas-node-$(VERSION_NUM)-1.$(subst -,_,$(GIT_BRANCH))_$(subst -,_,$(VARIANT)).$(shell date +%Y%m%d)git$(GIT_REV).tar.gz

DEPLOY_USER ?= acs
DEPLOY_HOST ?= villas.fein-aachen.org
DEPLOY_PATH ?= /var/www/villas/node

DOCKER_IMAGE ?= villas/node
DOCKER_TAG ?= $(GIT_BRANCH)

deploy-docker-dev: DOCKER_IMAGE := $(DOCKER_IMAGE)-dev
docker-dev: DOCKER_IMAGE := $(DOCKER_IMAGE)-dev

packaging: rpm dist
	
deploy: deploy-dist deploy-rpm deploy-docker
	
# Source tarballs
dist: $(TAR_VILLAS)

$(TAR_VILLAS): | $$(dir $$@)
	tar $(TAROPTS) -C $(SRCDIR) -czf $@ .

deploy-dist: $(TAR_VILLAS)
	rsync $(TAR_VILLAS) $(DEPLOY_USER)@$(DEPLOY_HOST):$(DEPLOY_PATH)/src
	
deploy-rpm:
	rsync -a --progress $(RPMDIR)/RPMS/ $(DEPLOY_USER)@$(DEPLOY_HOST):$(DEPLOY_PATH)/../packages/
	ssh $(DEPLOY_USER)@$(DEPLOY_HOST) createrepo $(DEPLOY_PATH)/../packages

deploy-docker deploy-docker-dev:
	docker push $(DOCKER_IMAGE):$(DOCKER_TAG)

# Docker targets
run-docker-dev: docker-dev
	docker run -it -p 80:80 -p 443:443 -p 1234:1234 -p 12000:12000/udp -p 12001:12001/udp --privileged --security-opt seccomp:unconfined -v "$(SRCDIR):/villas" $(DOCKER_IMAGE)-dev:$(GIT_BRANCH)

docker-dev:
	docker pull fedora:25
	docker build -f Dockerfile.dev -t $(DOCKER_IMAGE):$(DOCKER_TAG) $(SRCDIR)
	
docker:
	docker pull fedora:25
	docker build -f Dockerfile -t $(DOCKER_IMAGE):$(DOCKER_TAG) $(SRCDIR)
	
clean-packaging:
	rm -f $(BUILDDIR)/packaging/villas-node-$(VERSION_NUM).tar.gz

install-packaging:

.PHONY: packaging install-packaging clean-packaging deploy deploy-dist deploy-rpm dist docker docker-dev run-docker-dev $(TAR_VILLAS)

-include packaging/rpm/Makefile.inc
