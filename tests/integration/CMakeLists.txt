# CMakeLists.txt.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

add_custom_target(run-integration-tests
    COMMAND
        /bin/bash -o pipefail -c \"
            SRCDIR=${PROJECT_SOURCE_DIR}
            BUILDDIR=${PROJECT_BINARY_DIR}
            ${PROJECT_SOURCE_DIR}/tools/integration-tests.sh 2>&1 | c++filt\"
    USES_TERMINAL
    DEPENDS
        villas-node
        villas-pipe
        villas-signal
        villas-hook
)

add_dependencies(run-tests run-integration-tests)
