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

add_library(villas-common SHARED
	advio.c
	bitset.c
	buffer.c
	json_buffer.cpp
	compat.c
	crypt.c
	hash_table.c
	hist.c
	kernel/kernel.c
	kernel/kernel.cpp
	kernel/rt.cpp
	list.c
	log.cpp
	log_legacy.cpp
	memory.cpp
	memory_manager.cpp
	plugin.cpp
	table.c
	task.c
	timing.c
	utils.c
	utils.cpp
	cpuset.cpp
	terminal.cpp
	version.cpp
	common.c
)

execute_process( 
	COMMAND uname -m
	COMMAND tr -d '\n'
	OUTPUT_VARIABLE ARCH
)

if(ARCH STREQUAL "x86_64")
	target_sources(villas-common PRIVATE tsc.c)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
	target_sources(villas-common PRIVATE
		kernel/pci.c
		kernel/vfio.cpp
	)
endif()

target_include_directories(villas-common PUBLIC
	${OPENSSL_INCLUDE_DIR}
	${CURL_INCLUDE_DIRS}
	${PROJECT_BINARY_DIR}/include
	${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(villas-common PUBLIC
	PkgConfig::JANSSON
	PkgConfig::LIBCONFIG
	${OPENSSL_LIBRARIES}
	${CURL_LIBRARIES}
	${CMAKE_DL_LIBS}
	spdlog
	fmt
	stdc++
)

target_compile_definitions(villas-common PUBLIC
	-D__STDC_FORMAT_MACROS -D_GNU_SOURCE
)

install(
    TARGETS villas-common
    EXPORT VILLASCommonConfig
    COMPONENT lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/include/villas/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/villas
    COMPONENT devel
    FILES_MATCHING
        PATTERN "*.h"
)

install(
    DIRECTORY ${PROJECT_BINARY_DIR}/include/villas/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/villas
    COMPONENT devel
    FILES_MATCHING
        PATTERN "*.h"
)
