#!/usr/bin/env bash
#
# Test example configurations
#
# Author: Steffen Vogel <post@steffenvogel.de>
# Author: Philipp Jungkamp <philipp@jungkamp.dev>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set -eo pipefail

cd "${SRCDIR}/etc"

export SKIP_REGEX='/(fpga|infiniband|opal-orchestra)\.(conf|json)$'

{
    # only test examples for node types that have been included in the build
    villas node -C | jq --raw-output0 '
      def examples(caps; $prefix): caps[] | $prefix + gsub("\\."; "-");
      examples(.hooks;   "examples/hooks/"),
      examples(.nodes;   "examples/nodes/"),
      examples(.formats; "examples/formats/")
    '

    # add other configurations explicitly using ls --zero
    # ls --zero ...
} | xargs -0 -n1 bash -c '
    base="$0"

    echo # prepend empty line

    for candidate in "${base}" "${base}.conf" "${base}.json"; do
        if [ ! -f "${candidate}" ]; then
            continue
        fi

        if [[ "${candidate}" =~ $SKIP_REGEX ]]; then
            echo "=== Skipping config: ${candidate}"
            exit 0
        fi

        echo "=== Testing config: ${candidate}"
        exec villas config -q -m "${candidate}"
    done

    echo "=== No config for: ${base}"
    exit 0
'
