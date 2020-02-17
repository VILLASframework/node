NAME ?= VILLASlive
RELEASEVER ?= 29
RELEASEARCH ?= x86_64
DATE = $(shell date +%Y-%m-%d)

DEPLOY_HOST ?= villas.fein-aachen.org
DEPLOY_USER ?= acs
DEPLOY_PATH ?= /var/www/villas/iso/beta/

ifneq ($(CI),)
	VERSION = $(CI_COMMIT_REF_NAME)
else
	VERSION = $(shell git describe --tags)
endif

BASE =       $(NAME)-$(RELEASEARCH)-$(RELEASEVER)
IMG =        $(BASE)-$(VERSION:v%=%).img
ISO =        $(BASE)-$(VERSION:v%=%).iso
IMG_LATEST = $(BASE)-latest.iso
ISO_LATEST = $(BASE)-latest.iso

SRCDIR := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
BUILDDIR ?= $(SRCDIR)/build

RSYNCOPTS ?= --recursive --ignore-missing-args --copy-links --chown $(DEPLOY_USER):$(DEPLOY_USER)

export BUILDDIR

FILES = $(shell find $(SRCDIR)/files -type f)
PATCHED_FILES = $(FILES:$(SRCDIR)/files/%=$(BUILDDIR)/patched_files/%)

# Tools
LMC = livemedia-creator --releasever=$(RELEASEVER) --project=$(NAME) --title=$(NAME)

# Targets
all: $(BUILDDIR)/$(IMG)

clean:
	rm -rf $(BUILDDIR)

deploy: $(BUILDDIR)/$(IMG)
	rsync $(RSYNCOPTS) $(BUILDDIR)/$(IMG) ChangeLog $(DEPLOY_USER}@$(DEPLOY_HOST):$(DEPLOY_PATH)
	ssh $(DEPLOY_USER)@$(DEPLOY_HOST) ln -s $(DEPLOY_PATH)/$(ISO) $(DEPLOY_PATH)/$(IMG_LATEST)

$(BUILDDIR)/$(IMG): $(BUILDDIR)/img/villas-live.img
	ln -s $< $@

$(BUILDDIR)/$(ISO): $(BUILDDIR)/iso/villas-live.iso
	ln -s $< $@

$(BUILDDIR)/iso/%.iso: $(SRCDIR)/%.ks $(PATCHED_FILES)
	# livemedia-creator requires that the resultdir is not present
	rm -rf $(dir $@)

	# livemedia-creator uses losetup to mount the image
	# When used in a Docker container, we must make sure
	# that there are some usable /dev/loopX devices.
	./ensure_loop.sh

	$(LMC) --make-iso --iso-only --iso-name=$(notdir $@) --resultdir=$(dir $@) --ks $<

$(BUILDDIR)/img/%.img: $(SRCDIR)/%.ks $(PATCHED_FILES) Fedora-Server-netinst-x86_64-29_Beta-1.5.iso
	# livemedia-creator requires that the resultdir is not present
	rm -rf $(dir $@)

	# livemedia-creator uses losetup to mount the image
	# When used in a Docker container, we must make sure
	# that there are some usable /dev/loopX devices.
	./ensure_loop.sh

	$(LMC) --make-disk --resultdir=$(dir $@) --ks $< --iso Fedora-Server-netinst-x86_64-29_Beta-1.5.iso

Fedora-Server-netinst-x86_64-29_Beta-1.5.iso:
	wget -nv https://download.fedoraproject.org/pub/fedora/linux/releases/test/29_Beta/Server/x86_64/iso/Fedora-Server-netinst-x86_64-29_Beta-1.5.iso


$(BUILDDIR)/patched_files/%: $(SRCDIR)/files/%
	@mkdir -p $(dir $@)
	sed -e "s/§VERSION§/$(VERSION)/g" \
	    -e "s/§RELEASEVER§/$(RELEASEVER)/g" \
	    -e "s/§RELEASEARCH§/$(RELEASEARCH)/g" \
	    -e "s/§DATE§/$(DATE)/g" < $< > $@

.PHONY: all deploy clean deploy

.PRECIOUS: %
