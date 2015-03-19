#!/bin/bash

mount /dev/sdb1 /media/usb

for part in dev sys proc; do
	umount /media/usb/$part
	mount -o bind /$part /media/usb/$part
done

chroot /media/usb
