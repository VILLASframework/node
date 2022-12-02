## CMakeLists.txt
#
# @author Daniel Krebs <github@daniel-krebs.net>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache License 2.0
#
# VILLAScommon
#
##############################################################################

add_library(villas-common SHARED
	base64.cpp
	buffer.cpp
	common.cpp
	compat.cpp
	cpuset.cpp
	dsp/pid.cpp
	hist.cpp
	kernel/kernel.cpp
	kernel/rt.cpp
	list.cpp
	log.cpp
	memory_manager.cpp
	memory.cpp
	plugin.cpp
	popen.cpp
	table.cpp
	task.cpp
	terminal.cpp
	timing.cpp
	tool.cpp
	utils.cpp
	uuid.cpp
	version.cpp
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	target_sources(villas-common PRIVATE tsc.cpp)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
	target_sources(villas-common PRIVATE
		kernel/pci.cpp
		kernel/vfio_device.cpp
		kernel/vfio_group.cpp
		kernel/vfio_container.cpp
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
	PkgConfig::UUID
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
