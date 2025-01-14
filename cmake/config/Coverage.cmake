# CMakeLists.txt.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set(CMAKE_CXX_FLAGS_COVERAGE
    "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage"
    CACHE STRING "Flags used by the C++ compiler during coverage builds."
    FORCE
)

set(CMAKE_C_FLAGS_COVERAGE
    "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage"
    CACHE STRING "Flags used by the C compiler during coverage builds."
    FORCE
)

set(CMAKE_EXE_LINKER_FLAGS_COVERAGE
    "${CMAKE_EXE_LINKER_FLAGS}"
    CACHE STRING "Flags used for linking binaries during coverage builds."
    FORCE
)

set(CMAKE_SHARED_LINKER_FLAGS_COVERAGE
    "${CMAKE_SHARED_LINKER_FLAGS_COVERAGE}"
    CACHE STRING "Flags used by the shared libraries linker during coverage builds."
    FORCE
)

mark_as_advanced(
    CMAKE_CXX_FLAGS_COVERAGE
    CMAKE_C_FLAGS_COVERAGE
    CMAKE_EXE_LINKER_FLAGS_COVERAGE
    CMAKE_SHARED_LINKER_FLAGS_COVERAGE
)

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    target_link_libraries("gcov")
endif()
