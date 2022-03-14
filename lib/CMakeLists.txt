# CMakeLists.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

add_compile_options(-fPIC)

list(APPEND INCLUDE_DIRS
    ${PROJECT_BINARY_DIR}/include
    ${PROJECT_SOURCE_DIR}/include
)

set(LIBRARIES
    villas-common
    PkgConfig::JANSSON
    PkgConfig::UUID
    m
    stdc++
    rt
)

set(LIB_SRC
    capabilities.cpp
    config_helper.cpp
    config.cpp
    dumper.cpp
    format.cpp
    mapping.cpp
    mapping_list.cpp
    memory.cpp
    memory/heap.cpp
    memory/managed.cpp
    memory/mmap.cpp
    node_direction.cpp
    node.cpp
    node_capi.cpp
    node_compat.cpp
    node_list.cpp
    path_destination.cpp
    path_source.cpp
    path.cpp
	path_list.cpp
    pool.cpp
    queue_signalled.cpp
    queue.cpp
    sample.cpp
    shmem.cpp
    signal_data.cpp
    signal_list.cpp
    signal_type.cpp
    signal.cpp
    socket_addr.cpp
    stats.cpp
    super_node.cpp
)

if(WITH_WEB)
    list(APPEND LIB_SRC
        web.cpp
    )

    list(APPEND LIBRARIES PkgConfig::LIBWEBSOCKETS)
endif()

if(WITH_GRAPHVIZ)
    list(APPEND LIBRARIES PkgConfig::CGRAPH PkgConfig::GVC)
endif()

if(WITH_LUA)
	list(APPEND INCLUDE_DIRS ${LUA_INCLUDE_DIR})
    list(APPEND LIBRARIES ${LUA_LIBRARIES})
endif()

if(WITH_NODE_INFINIBAND)
    list(APPEND LIB_SRC memory/ib.cpp)
endif()

add_subdirectory(nodes)
list(APPEND WHOLE_ARCHIVES nodes)

add_subdirectory(formats)
list(APPEND WHOLE_ARCHIVES formats)

if(WITH_CONFIG)
    list(APPEND INCLUDE_DIRS ${LIBCONFIG_INCLUDE_DIRS})
    list(APPEND LIBRARIES PkgConfig::LIBCONFIG)
endif()

if(WITH_HOOKS)
    list(APPEND LIB_SRC
        hook.cpp
        hook_list.cpp
    )

    add_subdirectory(hooks)
    list(APPEND WHOLE_ARCHIVES hooks)
endif()

if(WITH_API AND WITH_WEB)
    list(APPEND LIB_SRC
        api.cpp
    )

    add_subdirectory(api)
    list(APPEND WHOLE_ARCHIVES api)
endif()

# libnl3 is optional but required for network emulation and IRQ pinning
if(LIBNL3_ROUTE_FOUND)
    list(APPEND LIB_SRC
        kernel/nl.cpp
        kernel/tc.cpp
        kernel/tc_netem.cpp
        kernel/if.cpp
    )

    list(APPEND INCLUDE_DIRS ${LIBNL3_ROUTE_INCLUDE_DIRS})
    list(APPEND LIBRARIES PkgConfig::LIBNL3_ROUTE)
endif()

if(LIBUSB_FOUND)
	list(APPEND LIB_SRC usb.cpp)
	list(APPEND LIBRARIES PkgConfig::LIBUSB)
endif()

if(WITH_FPGA)
    list(APPEND LIBRARIES villas-fpga)
endif()

add_library(villas SHARED ${LIB_SRC})
target_include_directories(villas PUBLIC ${INCLUDE_DIRS})
target_link_libraries(villas PUBLIC ${LIBRARIES})
target_link_libraries(villas PRIVATE -Wl,--whole-archive ${WHOLE_ARCHIVES} -Wl,--no-whole-archive)

# We need to link with -Bsymbolic in order to use link libvillas
# with Go code (Cgo)
# See also: https://stackoverflow.com/a/67299849
# target_link_options(villas PUBLIC "-Wl,-Bsymbolic")

set_target_properties(villas PROPERTIES
    VERSION ${CMAKE_PROJECT_VERSION}
    SOVERSION 1
)

install(
    TARGETS villas
    EXPORT VILLASNodeConfig
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
