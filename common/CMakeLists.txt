## CMakeLists.txt
#
# @author Daniel Krebs <github@daniel-krebs.net>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache License 2.0
#
# VILLAScommon
#
##############################################################################

cmake_minimum_required(VERSION 3.14)

project(villas-common
    VERSION 0.0.0
    DESCRIPTION "VILLAScommon"
    HOMEPAGE_URL "https://github.com/VILLASframework/common"
    LANGUAGES C CXX
)

# Some more project settings
set(PROJECT_AUTHOR "Steffen Vogel")
set(PROJECT_COPYRIGHT "2014-2021, Institute for Automation of Complex Power Systems, RWTH Aachen University")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(TOPLEVEL_PROJECT ON)
else()
    set(TOPLEVEL_PROJECT OFF)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

include(FindPkgConfig)
include(FeatureSummary)
include(GNUInstallDirs)
include(GetVersion)

# Get version info and buildid from Git
if(TOPLEVEL_PROJECT)
    GetVersion(${PROJECT_SOURCE_DIR} "CMAKE_PROJECT")
endif()

# Check packages
find_package(OpenSSL 1.0.0 REQUIRED)
find_package(CURL 7.29 REQUIRED)
find_package(spdlog 1.6.0 REQUIRED)
find_package(fmt 5.3.0 REQUIRED)
find_package(Criterion)

pkg_check_modules(JANSSON IMPORTED_TARGET REQUIRED jansson>=2.7)
pkg_check_modules(LIBCONFIG IMPORTED_TARGET libconfig>=1.4.9)
pkg_check_modules(UUID IMPORTED_TARGET REQUIRED uuid>=2.23)

add_subdirectory(lib)
if(CRITERION_FOUND AND TOPLEVEL_PROJECT)
    add_subdirectory(tests)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/villas/config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/villas/config.hpp
)

if(TOPLEVEL_PROJECT)
    feature_summary(WHAT ALL VAR FEATURES)
    message(STATUS "Building ${CMAKE_PROJECT_DESCRIPTION}:")
    message(STATUS "${FEATURES}")

    if(FOUND_GIT_VERSION)
        message(STATUS "  VERSION:         ${CMAKE_PROJECT_VERSION}")
        message(STATUS "  RELEASE:         ${CMAKE_PROJECT_RELEASE}")
        message(STATUS "  GIT_REV:         ${CMAKE_PROJECT_GIT_REV}")
        message(STATUS "  GIT_BRANCH:      ${CMAKE_PROJECT_GIT_BRANCH}")
        message(STATUS "  VARIANT:         ${CMAKE_PROJECT_VARIANT}")
        message(STATUS "  BUILD_ID:        ${CMAKE_PROJECT_BUILD_ID}")
        message(STATUS "  BUILD_DATE:      ${CMAKE_PROJECT_BUILD_DATE}")
    endif()

    message(STATUS "  ARCH:            ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "  OS:              ${CMAKE_SYSTEM_NAME}")
endif()
