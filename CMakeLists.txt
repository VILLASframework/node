# Main CMakeLists.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

project(VILLASnode C CXX)

# Several CMake settings/defaults
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(CMAKE_SOURCE_DIR STREQUAL VILLASnode_SOURCE_DIR)
    set(TOPLEVEL_PROJECT ON)
endif()

if(APPLE)
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/opt/local/lib/pkgconfig")
endif()

include(FindPkgConfig)
include(CheckIncludeFile)
include(FeatureSummary)
include(GNUInstallDirs)

# Compiler flags
if(BUILD32)
    add_compile_options(-m32)
    link_libraries(-m32)
endif()

if(APPLE)
    add_definitions(-D_DARWIN_C_SOURCE)
endif()

add_definitions(-D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE)
add_compile_options(-fdiagnostics-color=auto)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Werror)
endif()

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
find_package(Libwebsockets 2.3.0 REQUIRED)
find_package(OpenSSL 1.0.0 REQUIRED)
find_package(CURL 7.29 REQUIRED)
find_package(Mosquitto)
find_package(Opal)
find_package(IBVerbs)
find_package(RDMACM)

# Check programs
find_program(PROTOBUFC_COMPILER NAMES protoc-c)
find_program(PROTOBUF_COMPILER NAMES protoc)

set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/share/pkgconfig:/usr/lib64/pkgconfig")

pkg_check_modules(PROTOBUF IMPORTED_TARGET protobuf>=2.6.0)
pkg_check_modules(PROTOBUFC IMPORTED_TARGET libprotobuf-c>=1.1.0)
pkg_check_modules(CRITERION IMPORTED_TARGET criterion>=2.3.1)
pkg_check_modules(JANSSON IMPORTED_TARGET REQUIRED jansson>=2.7)
pkg_check_modules(LIBNL3_ROUTE IMPORTED_TARGET libnl-route-3.0>=3.2.27)
pkg_check_modules(LIBIEC61850 IMPORTED_TARGET libiec61850>=1.2)
pkg_check_modules(LIBCONFIG IMPORTED_TARGET libconfig>=1.4.9)
pkg_check_modules(RABBITMQ_C IMPORTED_TARGET librabbitmq>=0.8.0)
pkg_check_modules(COMEDILIB IMPORTED_TARGET comedilib>=0.11.0)
pkg_check_modules(LIBZMQ IMPORTED_TARGET libzmq>=2.2.0)
pkg_check_modules(NANOMSG IMPORTED_TARGET nanomsg)
if(NOT NANOMSG_FOUND)
    pkg_check_modules(NANOMSG IMPORTED_TARGET libnanomsg>=1.0.0)
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

set(V 2)
set(PREFIX ${CMAKE_INSTALL_PREFIX})

if(CMAKE_BUILD_TYPE)
    string(TOLOWER "${CMAKE_BUILD_TYPE}" VARIANT)
else()
    set(VARIANT "release")
endif()

# Add more build configurations
include(cmake/config/Debug.cmake)
include(cmake/config/Release.cmake)
include(cmake/config/Coverage.cmake)
include(cmake/config/Profiling.cmake)

# Add git revision and build variant defines
set(PROJECT_AUTHOR "Steffen Vogel")
set(PROJECT_COPYRIGHT "2018, Institute for Automation of Complex Power Systems, RWTH Aachen University")

set(PROJECT_SOVERSION 1)

execute_process(
    COMMAND git describe --tags --abbrev=0 --match "v*"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE PROJECT_VERSION_STR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(REGEX REPLACE "^v([0-9]+\\.[0-9]+\\.[0-9]+)$" "\\1"     PROJECT_VERSION       ${PROJECT_VERSION_STR})
string(REGEX REPLACE "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$" "\\1" PROJECT_MAJOR_VERSION ${PROJECT_VERSION_STR})
string(REGEX REPLACE "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$" "\\2" PROJECT_MINOR_VERSION ${PROJECT_VERSION_STR})
string(REGEX REPLACE "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$" "\\3" PROJECT_PATCH_VERSION ${PROJECT_VERSION_STR})

string(TIMESTAMP BUILD_DATE "%Y%m%d")

if(DEFINED ENV{CI})
	string(APPEND VARIANT "-ci")
    string(SUBSTRING $ENV{CI_COMMIT_SHA} 0 7 GIT_REV)
	set(GIT_BRANCH $ENV{CI_COMMIT_REF_NAME})
else()
    execute_process(
        COMMAND git rev-parse --short=7 HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_REV
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

if(DEFINED ENV{CI_COMMIT_TAG})
		set(PROJECT_RELEASE 1)
else()
    string(REPLACE "-" "_" GIT_BRANCH_NORM ${GIT_BRANCH})
    string(REPLACE "-" "_" VARIANT_NORM    ${VARIANT})

    set(PROJECT_RELEASE "1.${GIT_BRANCH_NORM}_${VARIANT_NORM}.${BUILD_DATE}git${GIT_REV}")
endif()

set(BUILDID "v${PROJECT_MAJOR_VERSION}.${PROJECT_MINOR_VERSION}.${PROJECT_PATCH_VERSION}-${GIT_REV}-${VARIANT}")

add_subdirectory(etc)
add_subdirectory(lib)
add_subdirectory(web)
add_subdirectory(packaging)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/villas/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/villas/config.h
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

if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
    feature_summary(WHAT ALL VAR FEATURES)
    message(STATUS "${FEATURES}")
    message(STATUS "Building VILLASnode: ${BUILDID}")
endif()

include(VILLASnodePackaging)
