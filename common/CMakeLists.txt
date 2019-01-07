## CMakeLists.txt
#
# @author Daniel Krebs <github@daniel-krebs.net>
# @copyright 2018, RWTH Institute for Automation of Complex Power Systems (ACS)
# @license GNU General Public License (version 3)
#
# VILLAScommon
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
##############################################################################

cmake_minimum_required(VERSION 3.7)

project(villas-common
    DESCRIPTION "VILLAScommon"
    LANGUAGES C CXX
)

# Some more project settings
set(PROJECT_AUTHOR "Steffen Vogel")
set(PROJECT_COPYRIGHT "2018, Institute for Automation of Complex Power Systems, RWTH Aachen University")
set(PROJECT_HOMEPAGE_URL "https://git.rwth-aachen.de/acs/public/villas/VILLAScommon")

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
find_package(Criterion)

set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)
set(SPDLOG_BUILD_TESTING OFF CACHE BOOL "Build spdlog tests" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "Build spdlog benchmarks" FORCE)
add_subdirectory(thirdparty/spdlog)

set(FMT_DOC OFF CACHE BOOL "Generate the doc target." FORCE)
set(FMT_INSTALL OFF CACHE BOOL "Generate the install target." FORCE)
set(FMT_TEST OFF CACHE BOOL "Generate the test target." FORCE)
add_subdirectory(thirdparty/fmtlib)

target_compile_options(fmt PUBLIC -fPIC)

pkg_check_modules(JANSSON IMPORTED_TARGET REQUIRED jansson>=2.7)
pkg_check_modules(LIBCONFIG IMPORTED_TARGET libconfig>=1.4.9)

add_subdirectory(lib)
if(CRITERION_FOUND)
    add_subdirectory(tests)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/villas/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/villas/config.h
)

if(TOPLEVEL_PROJECT)
    feature_summary(WHAT ALL VAR FEATURES)
    message(STATUS "${FEATURES}")
    message(STATUS "Building ${PROJECT_DESCRIPTION}:")
    message(STATUS "  VERSION:         ${CMAKE_PROJECT_VERSION}")
    message(STATUS "  RELEASE:         ${CMAKE_PROJECT_RELEASE}")
    message(STATUS "  GIT_REV:         ${CMAKE_PROJECT_GIT_REV}")
    message(STATUS "  GIT_BRANCH:      ${CMAKE_PROJECT_GIT_BRANCH}")
    message(STATUS "  VARIANT:         ${CMAKE_PROJECT_VARIANT}")
    message(STATUS "  BUILD_ID:        ${CMAKE_PROJECT_BUILD_ID}")
    message(STATUS "  BUILD_DATE:      ${CMAKE_PROJECT_BUILD_DATE}")
endif()
