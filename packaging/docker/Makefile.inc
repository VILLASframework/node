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

DOCKER = docker

DOCKER_FILE ?= Dockerfile
DOCKER_IMAGE ?= villas/node
DOCKER_TAG ?= $(GIT_BRANCH)

DOCKER_TARGETS = docker-app docker-dev docker-dev-centos docker-dev-ubuntu
DOCKER_RUN_TARGETS = $(addprefix run-,$(DOCKER_TARGETS))
DOCKER_DEPLOY_TARGETS = $(addprefix deploy-,$(DOCKER_TARGETS)) 

DOCKER_RUN_OPTS = --interactive --tty \
	--port 80:80 --port 443:443 --port 12000:12000/udp --port 12001:12001/udp \
	--privileged --security-opt seccomp:unconfined --volume "$(SRCDIR):/villas"
	
# Special cases for 'docker'target
run-docker: run-docker-app
deploy-docker: deploy-docker-app
docker: docker-app
	$(DOCKER) tag $(DOCKER_IMAGE)-app:$(DOCKER_TAG) $(DOCKER_IMAGE):$(DOCKER_TAG)

.PHONY: docker run-docker deploy-docker

$(DOCKER_DEPLOY_TARGETS): deploy-docker-%: docker-%
	$(DOCKER) push $(DOCKER_IMAGE)-%*:$(DOCKER_TAG)

$(DOCKER_RUN_TARGETS): run-docker-%: docker-%
	$(DOCKER)  run $(DOCKER_RUN_OPTS) $(DOCKER_IMAGE)-$*:$(DOCKER_TAG)

$(DOCKER_TARGETS): docker-%: $(BUILDDIR)/packaging/docker/Image.%

# The docker build targets use the --iidfile option to write the newly build image
# id to an file. Make is using the modification timestamp of this file and the Dockerfile
# to determine when the image needs to be rebuild.
$(BUILDDIR)/packaging/docker/Image.%: packaging/docker/Dockerfile.% | $(BUILDDIR)/packaging/docker/
	$(DOCKER) build --pull \
		--file $< --iidfile $@ \
		--tag $(DOCKER_IMAGE)-$*:$(DOCKER_TAG) \
		--label org.label-schema.vcs-ref="$(GIT_REV)" \
		--label org.label-schema.vcs-branch="$(GIT_BRANCH)" \
		--label org.label-schema.version="$(VERSION)" \
		--label org.label-schema.variant="$(VARIANT)" \
		$(SRCDIR)

.PHONY: $(DOCKER_TARGETS) $(DOCKER_RUN_TARGETS) $(DOCKER_DEPLOY_TARGETS)
