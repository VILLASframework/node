## CMakeLists.txt
#
# Author: Daniel Krebs <github@daniel-krebs.net>
# SPDX-FileCopyrightText: 2018 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

include(FindPkgConfig)

set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/share/pkgconfig:/usr/lib64/pkgconfig")

pkg_check_modules(JANSSON REQUIRED IMPORTED_TARGET jansson)

find_package(Threads)
find_package(Criterion)

include_directories(thirdparty/CLI11)
include_directories(thirdparty/rang)

add_subdirectory(thirdparty/libxil)
add_subdirectory(lib)
add_subdirectory(src)

if(CRITERION_FOUND)
    add_subdirectory(tests/unit)
endif()

# Project settings
set(PROJECT_NAME "VILLASfpga")
set(PROJECT_DESCRIPTION "Host library for configuring and communicating with VILLASfpga")
set(PROJECT_VENDOR "Institute for Automation of Complex Power Systems, RWTH Aachen University")
set(PROJECT_URL "https://www.fein-aachen.org/projects/villas-fpga/")
set(PROJECT_VERSION_MAJOR "0")
set(PROJECT_VERSION_MINOR "1")
set(PROJECT_VERSION_PATCH "0")
set(PROJECT_RELEASE "1")

# pkg-config
configure_file("libvillas-fpga.pc.in" "libvillas-fpga.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libvillas-fpga.pc" DESTINATION "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/pkgconfig")

# As close as possible to Fedoras naming
set(CPACK_PACKAGE_FILE_NAME "${CPACK_SOURCE_PACKAGE_FILE_NAME}-${CPACK_RPM_PACKAGE_RELEASE}.${CPACK_RPM_PACKAGE_ARCHITECTURE}")

set(CPACK_GENERATOR "RPM")
include(CPack)
