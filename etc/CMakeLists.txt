# CMakeLists.txt.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMPONENT bin
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/villas/node
    FILES_MATCHING
        PATTERN "*.conf"
        PATTERN "*.json"
)
