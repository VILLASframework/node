# Main CMakeLists.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASnode
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
###################################################################################

cmake_minimum_required(VERSION 3.6)

project(villas-node 
    DESCRIPTION "VILLASnode"
    LANGUAGES C CXX
)

# Some more project settings
set(PROJECT_AUTHOR "Steffen Vogel")
set(PROJECT_COPYRIGHT "2017-2019, Institute for Automation of Complex Power Systems, RWTH Aachen University")
set(PROJECT_HOMEPAGE_URL "https://www.fein-aachen.org/projects/villas-node/")

# Several CMake settings/defaults
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake;${PROJECT_SOURCE_DIR}/common/cmake")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(TOPLEVEL_PROJECT ON)
else()
    set(TOPLEVEL_PROJECT OFF)
endif()

if(APPLE)
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/opt/local/lib/pkgconfig")
endif()

include(FindPkgConfig)
include(CheckIncludeFile)
include(FeatureSummary)
include(GNUInstallDirs)
include(GetVersion)

# Compiler flags
if(BUILD32)
    add_compile_options(-m32)
    link_libraries(-m32)
endif()

if(APPLE)
    add_definitions(-D_DARWIN_C_SOURCE)
endif()

add_definitions(-D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE)
add_compile_options(-Wall -Werror -fdiagnostics-color=auto)

# Check OS
check_include_file("sys/eventfd.h" HAS_EVENTFD)
check_include_file("semaphore.h" HAS_SEMAPHORE)
check_include_file("sys/mman.h" HAS_MMAN)

# Use the switch NO_EVENTFD to deactivate eventfd usage indepentent of availability on OS
if(${NO_EVENTFD})
    set(HAS_EVENTFD OFF)
endif()

# Check packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(Mosquitto)
find_package(Opal)
find_package(IBVerbs)
find_package(RDMACM)
find_package(spdlog)

# Check programs
find_program(PROTOBUFC_COMPILER NAMES protoc-c)
find_program(PROTOBUF_COMPILER NAMES protoc)

set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/share/pkgconfig:/usr/lib64/pkgconfig")

pkg_check_modules(JANSSON IMPORTED_TARGET REQUIRED jansson>=2.7)
pkg_check_modules(LIBWEBSOCKETS IMPORTED_TARGET REQUIRED libwebsockets>=2.3.0)
pkg_check_modules(PROTOBUF IMPORTED_TARGET protobuf>=2.6.0)
pkg_check_modules(PROTOBUFC IMPORTED_TARGET libprotobuf-c>=1.1.0)
pkg_check_modules(CRITERION IMPORTED_TARGET criterion>=2.3.1)
pkg_check_modules(LIBNL3_ROUTE IMPORTED_TARGET libnl-route-3.0>=3.2.27)
pkg_check_modules(LIBIEC61850 IMPORTED_TARGET libiec61850>=1.2)
pkg_check_modules(LIBCONFIG IMPORTED_TARGET libconfig>=1.4.9)
pkg_check_modules(RABBITMQ_C IMPORTED_TARGET librabbitmq>=0.8.0)
pkg_check_modules(COMEDILIB IMPORTED_TARGET comedilib>=0.11.0)
pkg_check_modules(LIBZMQ IMPORTED_TARGET libzmq>=2.2.0)
pkg_check_modules(LIBULDAQ IMPORTED_TARGET libuldaq>=1.0.0)
pkg_check_modules(NANOMSG IMPORTED_TARGET nanomsg)
if(NOT NANOMSG_FOUND)
    pkg_check_modules(NANOMSG IMPORTED_TARGET libnanomsg>=1.0.0)
endif()
pkg_check_modules(RE IMPORTED_TARGET re)
if(NOT RE_FOUND)
    pkg_check_modules(RE IMPORTED_TARGET libre>=0.5.9)
endif()

# Build options
option(WITH_HOOKS   "Build with support for processing hook plugins" ON)
option(WITH_IO      "Build with support format plugins" ON)
option(WITH_WEB     "Build with internal webserver" ON)
option(WITH_API     "Build with remote control API" ON)
option(WITH_CONFIG  "Build with support for libconfig configuration syntax" ON)
option(WITH_SRC     "Build villas-* executables" ${TOPLEVEL_PROJECT})
option(WITH_TOOLS   "Build auxilary tools" ${TOPLEVEL_PROJECT})
option(WITH_TESTS   "Run tests" ${TOPLEVEL_PROJECT})
option(WITH_PLUGINS "Build plugins" ${TOPLEVEL_PROJECT})
option(WITH_CLIENTS "Build client applications" ${TOPLEVEL_PROJECT})
option(WITH_DOC     "Build documentation" ${TOPLEVEL_PROJECT})

# Add more build configurations
include(cmake/config/Debug.cmake)
include(cmake/config/Release.cmake)
include(cmake/config/Coverage.cmake)
include(cmake/config/Profiling.cmake)

# Get version info and buildid from Git
GetVersion(${PROJECT_SOURCE_DIR} "CMAKE_PROJECT")

add_subdirectory(common)
add_subdirectory(etc)
add_subdirectory(lib)
add_subdirectory(web)
add_subdirectory(packaging)
add_subdirectory(python)

if(WITH_SRC)
    add_subdirectory(src)
endif()

if(WITH_TOOLS)
    add_subdirectory(tools)
endif()

if(WITH_PLUGINS)
    add_subdirectory(plugins)
endif()

if(WITH_DOC)
    add_subdirectory(doc)
endif()

if(WITH_CLIENTS)
    add_subdirectory(clients)
endif()

if(WITH_TESTS)
    add_subdirectory(tests)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/villas/node/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/villas/node/config.h
)

# Show feature summary
add_feature_info(HOOKS   WITH_HOOKS   "Build with support for processing hook plugins")
add_feature_info(IO      WITH_IO      "Build with support format plugins")
add_feature_info(WEB     WITH_WEB     "Build with internal webserver")
add_feature_info(API     WITH_API     "Build with remote control API")
add_feature_info(CONFIG  WITH_CONFIG  "Build with support for libconfig configuration syntax")
add_feature_info(TOOLS   WITH_TOOLS   "Build auxilary tools")
add_feature_info(TESTS   WITH_TESTS   "Run tests")
add_feature_info(PLUGINS WITH_PLUGINS "Build plugins")
add_feature_info(CLIENTS WITH_CLIENTS "Build client applications")
add_feature_info(DOC     WITH_DOC     "Build documentation")

if(TOPLEVEL_PROJECT)
    feature_summary(WHAT ALL VAR FEATURES)
    message(STATUS "${FEATURES}")
    message(STATUS "Building ${CMAKE_PROJECT_DESCRIPTION}:")
    message(STATUS "  VERSION:         ${CMAKE_PROJECT_VERSION}")
    message(STATUS "  RELEASE:         ${CMAKE_PROJECT_RELEASE}")
    message(STATUS "  GIT_REV:         ${CMAKE_PROJECT_GIT_REV}")
    message(STATUS "  GIT_BRANCH:      ${CMAKE_PROJECT_GIT_BRANCH}")
    message(STATUS "  VARIANT:         ${CMAKE_PROJECT_VARIANT}")
    message(STATUS "  BUILD_ID:        ${CMAKE_PROJECT_BUILD_ID}")
    message(STATUS "  BUILD_DATE:      ${CMAKE_PROJECT_BUILD_DATE}")
endif()

include(VILLASnodePackaging)
