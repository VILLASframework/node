# CMakeLists.txt.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

add_executable(villas-shmem villas-shmem.cpp)

target_link_libraries(villas-shmem PUBLIC villas)

install(
    TARGETS villas-shmem
    COMPONENT bin
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
