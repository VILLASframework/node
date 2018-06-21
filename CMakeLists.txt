# Main CMakeLists.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

cmake_minimum_required(VERSION 3.0)

project(VILLASnode)

set(CMAKE_C_STANDARD 11)

set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH ON)

find_package(PkgConfig)
include(CheckIncludeFile)

# Check OS
check_include_file("sys/eventfd.h" HAS_EVENTFD)

# Build options
option(WITH_HOOKS "Build with support for processing hook plugins" ON)
option(WITH_IO "Build with support format plugins" ON)
option(WITH_WEB "Build with internal webserver" ON)
option(WITH_API "Build with remote control API" ON)
option(WITH_CONFIG "Build with support for libconfig configuration syntax" ON)

set(V 2)
set(PREFIX ${CMAKE_INSTALL_PREFIX})

set(VARIANT "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# Add git revision and build variant defines
execute_process(
    COMMAND git describe --tags --abbrev=0 --match "v*"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(SUBSTRING ${VERSION} 2 -1 VERSION_NUM)

string(TIMESTAMP BUILD_DATE "%Y%m%d")

if(DEFINED ENV{CI})
	string(APPEND VARIANT "-ci")
    string(SUBSTRING $ENV{CI_COMMIT_SHA} 0 7 GIT_REV)
	set(GIT_BRANCH ${CI_COMMIT_REF_NAME})
else()
    execute_process(
        COMMAND git rev-parse --short=7 HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_REV
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

if(DEFINED ENV{CI_COMMIT_TAG})
		set(RELEASE 1)
else()
    string(REPLACE "-" "_" GIT_BRANCH_NORM ${GIT_BRANCH})
    string(REPLACE "-" "_" VARIANT_NORM    ${VARIANT})

    set(RELEASE "1.${GIT_BRANCH_NORM}_${VARIANT_NORM}.${BUILD_DATE}git${GIT_REV}")
endif()

set(BUILDID "${VERSION}-${GIT_REV}-${VARIANT}")

configure_file(
    ${CMAKE_SOURCE_DIR}/include/villas/config.h.in
    ${CMAKE_BINARY_DIR}/include/villas/config.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(tools)
