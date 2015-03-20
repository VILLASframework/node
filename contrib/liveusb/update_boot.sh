#!/bin/sh

# author: Christian Berendt <mail@cberendt.net>

set -x

for kernel in $(find /boot/vmlinuz*); do
    version=$(basename $kernel)
    version=${version#*-}
    if [ ! -e /boot/initramfs-$version.img ]; then
        sudo /usr/bin/dracut /boot/initramfs-$version.img $version
    fi
done

for image in $(find /boot/initramfs*); do
    version=${image%.img}
    version=${version#*initramfs-}
    if [ ! -e /boot/vmlinuz-$version ]; then
        sudo rm $image
    fi
done

/usr/sbin/grub2-mkconfig -o /boot/grub2/grub.cfg

