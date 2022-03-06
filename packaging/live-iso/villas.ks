################################################################################
# Kickstart file for VILLAS installation
################################################################################

# Configuration
lang en_US.UTF-8
keyboard us
timezone Europe/Berlin
selinux --disabled
firewall --disabled
services --enabled=sshd,NetworkManager,chronyd,sshd,tuned,initial-setup
network --bootproto=dhcp --device=link --activate --hostname=villas
rootpw --plaintext villas-admin
shutdown

bootloader --timeout=1 --append "preempt=full"

zerombr
clearpart --all --initlabel --disklabel=msdos
part / --size=8192 --fstype ext4

# make sure that initial-setup runs and lets us do all the configuration bits
firstboot --reconfig

# Add repositories
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
dracut-live
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

# Some custom packages
tuned
tuned-profiles-realtime

# Tools
autoconf
automake
bash-completion
bind-utils
bison
chrony
cmake
curl
dia
doxygen
flex
gcc
gcc-c++
git
graphviz
iproute
jq
libtool
lshw
make
mercurial
nano
ninja-build
openssh-clients
pkgconfig
procps-ng
protobuf-c-compiler
protobuf-compiler
psmisc
python-pip
tar
texinfo
tmux
traceroute
wget

# Libraries and build-time dependencies of VILLASnode
openssl-devel
protobuf-devel
protobuf-c-devel
libuuid-devel
libconfig-devel
libnl3-devel
libcurl-devel
jansson-devel
spdlog-devel
fmt-devel
libwebsockets-devel
zeromq-devel
nanomsg-devel
librabbitmq-devel
mosquitto-devel
libibverbs-devel
librdmacm-devel
re-devel
libusb-devel
lua-devel
librdkafka-devel

%end

################################################################################
# Custom post installer
%post

# Install VILLASnode
mkdir /villas
git clone https://git.rwth-aachen.de/acs/public/villas/node.git /villas/node

cd /villas/node
git submodule update --init common

bash ./packaging/deps.sh

mkdir build
cd build

cmake ..
make -j$(nproc) install

echo /usr/local/lib64 > /etc/ld.so.conf.d/local.conf
ldconfig

# Select real-time tuned profile
PROC=$(nproc)
ISOLATED_CORES=
if ((PROC > 4)); then
  ISOLATED_CORES+=$(seq -s, $((PROC/2)) $((PROC-1)))
fi

echo isolated_cores=${ISOLATED_CORES} >> /etc/tuned/realtime-variables.conf
tuned-adm profile realtime

# Install patched files
cd /villas/node/packaging/live-iso
make patched
rsync --ignore-errors --archive --verbose /villas/node/packaging/live-iso/build/patched_files/ /
%end

# From fedora-disk-base
%post

# Find the architecture we are on
arch=$(uname -m)
 
releasever=$(rpm --eval '%{fedora}')
rpm --import /etc/pki/rpm-gpg/RPM-GPG-KEY-fedora-$releasever-primary

echo "Packages within this disk image"
rpm -qa --qf '%{size}\t%{name}-%{version}-%{release}.%{arch}\n' |sort -rn

# remove random seed, the newly installed instance should make it's own
rm -f /var/lib/systemd/random-seed

# The enp1s0 interface is a left over from the imagefactory install, clean this up
rm -f /etc/NetworkManager/system-connections/*.nmconnection
 
dnf -y remove dracut-config-generic

# Remove machine-id on pre generated images
rm -f /etc/machine-id
touch /etc/machine-id

# Note that running rpm recreates the rpm db files which aren't needed or wanted
rm -f /var/lib/rpm/__db*

%end
