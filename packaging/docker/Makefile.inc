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
	--publish 80:80 --publish 443:443 --publish 12000:12000/udp --publish 12001:12001/udp \
	--privileged --security-opt seccomp:unconfined --volume "$(SRCDIR):/villas"
	
# Special cases for 'docker'target
run-docker: run-docker-app
deploy-docker: deploy-docker-app
docker: docker-app
	$(DOCKER) tag $(DOCKER_IMAGE)-app:$(DOCKER_TAG) $(DOCKER_IMAGE):$(DOCKER_TAG)

.PHONY: docker run-docker deploy-docker

$(DOCKER_DEPLOY_TARGETS): deploy-docker-%: docker-%
	$(DOCKER) push $(DOCKER_IMAGE)-$*:$(DOCKER_TAG)

$(DOCKER_RUN_TARGETS): run-docker-%: docker-%
	$(DOCKER) run $(DOCKER_RUN_OPTS) $(DOCKER_IMAGE)-$*:$(DOCKER_TAG)

$(DOCKER_TARGETS): docker-%: $(BUILDDIR)/packaging/docker/Image.%

ifneq ($(CI_COMMIT_TAG),)
	DOCKER_OPTS += --tag $(DOCKER_IMAGE)-$*:$(CI_COMMIT_TAG)
endif

# The docker build targets use the --iidfile option to write the newly build image
# id to an file. Make is using the modification timestamp of this file and the Dockerfile
# to determine when the image needs to be rebuild.
$(BUILDDIR)/packaging/docker/Image.%: packaging/docker/Dockerfile.% | $(BUILDDIR)/packaging/docker/
	$(DOCKER) build \
		--file $< --iidfile $@ \
		--tag $(DOCKER_IMAGE)-$*:$(DOCKER_TAG) \
		--tag $(DOCKER_IMAGE)-$*:latest \
		--build-arg BUILDER_IMAGE=$(DOCKER_IMAGE)-dev:$(DOCKER_TAG) \
		--build-arg DOCKER_TAG=${DOCKER_TAG} \
		--build-arg GIT_BRANCH=${GIT_BRANCH} \
		--build-arg GIT_REV=${GIT_REV} \
		--build-arg VERSION=${VERSION} \
		--build-arg VARIANT=${VARIANT} \
		$(DOCKER_OPTS) $(SRCDIR)

.PHONY: $(DOCKER_TARGETS) $(DOCKER_RUN_TARGETS) $(DOCKER_DEPLOY_TARGETS)
