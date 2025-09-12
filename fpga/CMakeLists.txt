## CMakeLists.txt
#
# Author: Daniel Krebs <github@daniel-krebs.net>
# SPDX-FileCopyrightText: 2018 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

include_directories(thirdparty/CLI11)
include_directories(thirdparty/rang)

add_subdirectory(thirdparty/libxil)
add_subdirectory(lib)
add_subdirectory(src)

if(CRITERION_FOUND)
    add_subdirectory(tests/unit)
endif()
