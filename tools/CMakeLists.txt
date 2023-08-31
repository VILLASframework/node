# CMakeLists.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

install(
    TARGETS ${TOOLS}
    COMPONENT tools
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    PROGRAMS villas
    COMPONENT bin
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    RENAME villas
)

install(
    PROGRAMS villas-api.sh
    COMPONENT bin
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    RENAME villas-api
)
