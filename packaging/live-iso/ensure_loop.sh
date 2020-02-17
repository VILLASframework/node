#!/bin/bash
# Ensure that we have at least two usable loop devices inside our Docker container
#
# Source: https://github.com/jpetazzo/dind/issues/19#issuecomment-48859883

ensure_loop(){
	num="$1"
	dev="/dev/loop$num"
	if test -b "$dev"; then
		echo "$dev is a usable loop device."
		return 0
	fi

	echo "Attempting to create $dev for docker ..."
	if ! mknod -m660 $dev b 7 $num; then
		echo "Failed to create $dev!" 1>&2
		return 3
	fi

	return 0
}

LOOP_A=$(losetup -f)
LOOP_A=${LOOP_A#/dev/loop}
LOOP_B=$(expr $LOOP_A + 1)
LOOP_C=$(expr $LOOP_A + 2)

ensure_loop $LOOP_A
ensure_loop $LOOP_B
ensure_loop $LOOP_C

losetup -la
