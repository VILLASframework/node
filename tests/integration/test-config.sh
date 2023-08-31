
#!/bin/bash
#
# Test example configurations
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set -e

CONFIGS=$(find ${SRCDIR}/etc/ -name '*.conf' -o -name '*.json')

for CONFIG in ${CONFIGS}; do
    if [ "$(basename ${CONFIG})" == "opal.conf" ] ||
       [ "$(basename ${CONFIG})" == "fpga.conf" ] ||
       [ "$(basename ${CONFIG})" == "paths.conf" ] ||
       [ "$(basename ${CONFIG})" == "tricks.json" ] ||
       [ "$(basename ${CONFIG})" == "tricks.conf" ] ||
	   [ "$(basename ${CONFIG})" == "vc707_ips.conf" ] ||
       [ "$(basename ${CONFIG})" == "infiniband.conf" ] ||
       [ "$(basename ${CONFIG})" == "global.conf" ]; then
        echo "=== Skipping config: ${CONFIG}"
        continue
    fi

    echo "=== Testing config: ${CONFIG}"
    villas test-config -c ${CONFIG}
done
