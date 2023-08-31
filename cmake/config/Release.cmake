# CMakeLists.txt.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

# don't treat warnings as errors on normal release builds
if("${CMAKE_BUILD_TYPE}" STREQUAL "Release" AND NOT DEFINED VILLAS_COMPILE_WARNING_AS_ERROR)
    set(VILLAS_COMPILE_WARNING_AS_ERROR OFF)
endif()
