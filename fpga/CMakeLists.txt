## CMakeLists.txt
#
# Author: Daniel Krebs <github@daniel-krebs.net>
# SPDX-FileCopyrightText: 2018 Institute for Automation of Complex Power Systems, EONERC
# SPDX-License-Identifier: Apache-2.0
##############################################################################
 
cmake_minimum_required(VERSION 3.5)

project(VILLASfpga
	LANGUAGES C CXX
	VERSION 0.1.0
)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Several CMake settings/defaults
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(TOPLEVEL_PROJECT ON)
else()
    set(TOPLEVEL_PROJECT OFF)
endif()

# GPU library is optional, check for CUDA presence
include(CheckLanguage)
check_language(CUDA)

if(CMAKE_CUDA_COMPILER)
	add_subdirectory(gpu)
else()
	message("No CUDA support, not building GPU library")
endif()

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

if(CRITERION_FOUND AND TOPLEVEL_PROJECT)
	add_subdirectory(tests/unit)
endif()

if(NOT CMAKE_PROJECT_NAME STREQUAL "villas-node")
	add_subdirectory(common)
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

# CPack
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR ${PROJECT_VENDOR})
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/Apache-2.0.txt")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

set(CPACK_PACKAGE_NAME "villas-fpga")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
set(CPACK_SOURCE_IGNORE_FILES "/build/;/.git/;~$;${CPACK_SOURCE_IGNORE_FILES}")

set(CPACK_RPM_PACKAGE_RELEASE ${PROJECT_RELEASE})
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
set(CPACK_RPM_PACKAGE_URL ${PROJECT_URL})
set(CPACK_RPM_PACKAGE_REQUIRES "libxil")
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")

# As close as possible to Fedoras naming
set(CPACK_PACKAGE_FILE_NAME "${CPACK_SOURCE_PACKAGE_FILE_NAME}-${CPACK_RPM_PACKAGE_RELEASE}.${CPACK_RPM_PACKAGE_ARCHITECTURE}")

set(CPACK_GENERATOR "RPM")
include(CPack)
