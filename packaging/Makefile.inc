TAROPTS = --exclude-ignore-recursive=.distignore --transform='s|^\.|villas-node-$(VERSION_NUM)|' --show-transformed-names

TAR_VILLAS = $(BUILDDIR)/packaging/villas-node-$(VERSION_NUM)-1.$(GIT_BRANCH)_$(subst -,_,$(VARIANT)).$(shell date +%Y%m%d)git$(GIT_REV).tar.gz

DEPLOY_USER ?= root
DEPLOY_HOST ?= 188.166.166.227
DEPLOY_PATH ?= /var/www/villas/node

DOCKER_REGISTRY ?= docker.io
DOCKER_IMAGE ?= villas/node

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
	docker tag -f $(DOCKER_IMAGE):$(GIT_BRANCH) $(DOCKER_REGISTRY)/$(DOCKER_IMAGE):$(GIT_BRANCH)
	docker push $(DOCKER_REGISTRY)/$(DOCKER_IMAGE):$(GIT_BRANCH)

# Docker targets
run-docker-dev: docker-dev
	docker run -it -p 80:80 -p 443:443 -p 1234:1234 --privileged --security-opt seccomp:unconfined -v "$(PWD):/villas" $(DOCKER_IMAGE)-dev:$(GIT_BRANCH)

docker-dev:
	docker pull fedora:25
	docker build -f Dockerfile.dev -t $(DOCKER_IMAGE):$(GIT_BRANCH) $(SRCDIR)
	
docker:
	docker pull fedora:25
	docker build -f Dockerfile -t $(DOCKER_IMAGE):$(GIT_BRANCH) $(SRCDIR)
	
clean-packaging:
	rm -f $(BUILDDIR)/packaging/villas-node-$(VERSION_NUM).tar.gz

install-packaging:

.PHONY: packaging install-packaging clean-packaging deploy deploy-dist deploy-rpm dist docker docker-dev run-docker-dev $(TAR_VILLAS)

-include packaging/rpm/Makefile.inc