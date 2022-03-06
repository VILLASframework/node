NAME ?= VILLASlive
RELEASEVER ?= 35
RELEASEARCH ?= x86_64
DATE = $(shell date +%Y-%m-%d)

DEPLOY_PATH ?= https://files.fein-aachen.org/
DEPLOY_USER ?= webdav

ifneq ($(CI),)
	VERSION = $(CI_COMMIT_REF_NAME)
else
	VERSION = $(shell git describe --tags)
endif

BASE =       $(NAME)-$(RELEASEARCH)-$(RELEASEVER)
IMG =        $(BASE)-$(VERSION:v%=%).img
IMG_LATEST = $(BASE)-latest.iso

BASE_ISO=Fedora-Server-netinst-$(RELEASEARCH)-$(RELEASEVER)-1.2.iso

SRCDIR := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
BUILDDIR ?= $(SRCDIR)/build

export BUILDDIR

FILES = $(shell find $(SRCDIR)/files -type f)
PATCHED_FILES = $(FILES:$(SRCDIR)/files/%=$(BUILDDIR)/patched_files/%)

# Tools
LMC = livemedia-creator \
	--releasever=$(RELEASEVER) \
	--ram=8192 \
	--vcpus=16

# Targets
all: iso
iso: $(BUILDDIR)/$(ISO)

patched: $(PATCHED_FILES)

clean:
	rm -rf $(BUILDDIR)

deploy: #$(BUILDDIR)/$(ISO)
	rclone config create fein webdav url ${DEPLOY_PATH} vendor other user ${DEPLOY_USER} pass ${DEPLOY_PASS}
	rclone config show
	rclone copy $(BUILDDIR)/$(ISO) fein:villas/iso

$(BUILDDIR)/$(ISO): $(BUILDDIR)/iso/villas.iso
	ln -s $< $@

$(BUILDDIR)/iso/%.iso: $(SRCDIR)/%.ks /tmp/$(BASE_ISO)
	# livemedia-creator requires that the resultdir is not present
	rm -rf $(dir $@)

	# livemedia-creator uses losetup to mount the image
	# When used in a Docker container, we must make sure
	# that there are some usable /dev/loopX devices.
	./ensure_loop.sh

	$(LMC) \
		--make-iso \
		--iso-only \
		--iso-name=$(notdir $@) \
		--iso /tmp/$(BASE_ISO) \
		--resultdir=$(dir $@) \
		--kernel-args "inst.ks=file:/villas.ks" \
		--ks $<

/tmp/$(BASE_ISO):
	wget -nv -O $@ https://ftp.halifax.rwth-aachen.de/fedora/linux/releases/$(RELEASEVER)/Server/$(RELEASEARCH)/iso/$(BASE_ISO)

$(BUILDDIR)/patched_files/%: $(SRCDIR)/files/%
	@mkdir -p $(dir $@)
	sed -e "s/§VERSION§/$(VERSION)/g" \
	    -e "s/§RELEASEVER§/$(RELEASEVER)/g" \
	    -e "s/§RELEASEARCH§/$(RELEASEARCH)/g" \
	    -e "s/§DATE§/$(DATE)/g" < $< > $@

.PHONY: all deploy clean deploy patched iso

.PRECIOUS: %
