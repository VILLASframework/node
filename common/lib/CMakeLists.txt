## CMakeLists.txt
#
# @author Daniel Krebs <github@daniel-krebs.net>
# @copyright 2014-2020, RWTH Institute for Automation of Complex Power Systems (ACS)
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
	advio.cpp
	buffer.cpp
	json_buffer.cpp
	compat.cpp
	hist.cpp
	dsp/pid.cpp
	kernel/kernel.cpp
	kernel/rt.cpp
	list.cpp
	log.cpp
	log_legacy.cpp
	memory.cpp
	memory_manager.cpp
	plugin.cpp
	table.cpp
	task.cpp
	timing.cpp
	utils.cpp
	cpuset.cpp
	terminal.cpp
	version.cpp
	common.cpp
	tool.cpp
	base64.cpp
	popen.cpp
)

if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
	target_link_libraries(villas-common PUBLIC "-lstdc++fs")
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	target_sources(villas-common PRIVATE tsc.cpp)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
	target_sources(villas-common PRIVATE
		kernel/pci.cpp
		kernel/vfio.cpp
#		kernel/vfio_legacy.cpp
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
	${OPENSSL_LIBRARIES}
	${CURL_LIBRARIES}
	${CMAKE_DL_LIBS}
	spdlog::spdlog
	fmt::fmt
	stdc++
)

if(WITH_CONFIG)
	target_link_libraries(villas-common PUBLIC
		PkgConfig::LIBCONFIG
	)
endif()

target_compile_definitions(villas-common PUBLIC
	-D__STDC_FORMAT_MACROS -D_GNU_SOURCE
)

set_target_properties(villas-common PROPERTIES
    VERSION ${CMAKE_PROJECT_VERSION}
    SOVERSION 1
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
        PATTERN "*.hpp"
)

install(
    DIRECTORY ${PROJECT_BINARY_DIR}/include/villas/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/villas
    COMPONENT devel
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
)
