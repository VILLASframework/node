#!/bin/bash

dnf -y --refresh install \
	tuned-utils \
	tuned-profiles-realtime

# Isolate half of the available cores by default
if [ -z "${ISOLATED_CORES}" ]; then
	PROC=$(nproc)
	ISOLATED_CORES=
	if ((PROC > 4)); then
		ISOLATED_CORES+=$(seq -s, $((PROC/2)) $((PROC-1)))
	fi
fi
echo isolated_cores=${ISOLATED_CORES} >> /etc/tuned/realtime-variables.conf

# Enable dynamic preemption
grubby --update-kernel=ALL --remove-args="preempt=full"

# Select real-time tuned profile
tuned-adm profile realtime
