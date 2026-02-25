## CMakeLists.txt
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(lib)
if(WITH_TESTS)
    add_subdirectory(tests)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/villas/config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/villas/config.hpp
)
