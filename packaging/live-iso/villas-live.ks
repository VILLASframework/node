################################################################################
# VILLASlive image
################################################################################

# Configuration
lang en_US.UTF-8
keyboard us
timezone Europe/Berlin
auth --useshadow --passalgo=sha512
selinux --disabled
firewall --disabled
services --enabled=sshd,NetworkManager,chronyd,sshd,tuned,initial-setup
network --bootproto=dhcp --device=link --activate
rootpw --plaintext villas-admin
shutdown

# make sure that initial-setup runs and lets us do all the configuration bits
firstboot --reconfig

bootloader --timeout=1
zerombr
clearpart --all --initlabel --disklabel=msdos
part / --size=8192 --fstype ext4

# make sure that initial-setup runs and lets us do all the configuration bits
firstboot --reconfig

# Add repositories
repo --name=planet-ccrma --install --baseurl=http://ccrma.stanford.edu/planetccrma/mirror/fedora/linux/planetcore/28/$basearch/
repo --name=fein --install --baseurl=https://packages.fein-aachen.org/fedora/$releasever/$basearch/

repo --name=fedora --mirrorlist=https://mirrors.fedoraproject.org/mirrorlist?repo=fedora-$releasever&arch=$basearch
repo --name=updates --mirrorlist=https://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f$releasever&arch=$basearch
url --mirrorlist=https://mirrors.fedoraproject.org/mirrorlist?repo=fedora-$releasever&arch=$basearch

################################################################################
# Install packages
%packages
@core
@hardware-support

-@dial-up
-@input-methods
-@standard

rng-tools
initial-setup
glibc-langpack-en

# remove this in %post
dracut-config-generic
-dracut-config-rescue
# install tools needed to manage and boot arm systems
-uboot-images-armv7
-initial-setup-gui
-glibc-all-langpacks
-trousers
-gfs2-utils
-reiserfs-utils

# Intel wireless firmware assumed never of use for disk images
-iwl*
-ipw*
-usb_modeswitch
-generic-release*

kernel
kernel-modules
kernel-modules-extra

kernel-rt
kernel-rt-modules
kernel-rt-modules-extra

# Some custom packages
tuned
tuned-profiles-realtime

# Tools
jq
iproute
nano
ntp
lshw
traceroute
bind-utils
curl
tar
openssh-clients
python-pip
psmisc
procps-ng
tmux
wget
gcc
bash-completion

# For building Tinc-VPN
readline-devel
zlib-devel
openssl-devel
lzo-devel
systemd-devel

# VILLASnode
villas-node
villas-node-doc
villas-node-tools
villas-node-plugins

%end

################################################################################
# Custom post installer
%post

# Select tuned profile
tuned-adm profile realtime

%end

################################################################################
# Copy all files to ISO and fix permissions
%post --nochroot

export
mount

#set -x
#
#rsync --ignore-errors --archive --verbose $BUILDDIR/patched_files/ /mnt/sysimage/
#
#chmod 600 /mnt/sysimage/root/.ssh/id_rsa*
#chmod 755 /mnt/sysimage/usr/local/bin/remote-admin
#chmod 755 /mnt/sysimage/usr/local/bin/install-tinc
#chmod 755 /mnt/sysimage/usr/local/bin/tune-realtime

%end


# From fedora-disk-base
%post

releasever=$(rpm -q --qf '%{version}\n' fedora-release)
rpm --import /etc/pki/rpm-gpg/RPM-GPG-KEY-fedora-$releasever-primary
echo "Packages within this disk image"
rpm -qa
# Note that running rpm recreates the rpm db files which aren't needed or wanted
rm -f /var/lib/rpm/__db*

# remove random seed, the newly installed instance should make it's own
rm -f /var/lib/systemd/random-seed

# The enp1s0 interface is a left over from the imagefactory install, clean this up
rm -f /etc/sysconfig/network-scripts/ifcfg-enp1s0

dnf -y remove dracut-config-generic

# Disable network service here, as doing it in the services line
# fails due to RHBZ #1369794
/sbin/chkconfig network off

# Remove machine-id on pre generated images
rm -f /etc/machine-id
touch /etc/machine-id

%end
