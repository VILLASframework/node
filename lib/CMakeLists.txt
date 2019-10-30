# CMakeLists.
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

add_compile_options(-fPIC)

list(APPEND INCLUDE_DIRS
    ${JANSSON_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}/include
    ${PROJECT_SOURCE_DIR}/include
)

set(LIBRARIES
    PkgConfig::JANSSON
    PkgConfig::UUID
    villas-common
    m
    stdc++
)

if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
	list(APPEND LIBRARIES "-lstdc++fs")
endif()

set(LIB_SRC
    config_helper.cpp
    config.cpp
    mapping.cpp
    memory.cpp
    memory/heap.cpp
    memory/managed.cpp
    memory/mmap.cpp
    node_direction.cpp
    node_type.cpp
    node.cpp
    path_destination.cpp
    path_source.cpp
    path.cpp
    plugin.cpp
    pool.cpp
    queue_signalled.cpp
    queue.cpp
    sample.cpp
    shmem.cpp
    signal.cpp
    stats.cpp
    super_node.cpp
    socket_addr.cpp
    io.cpp
    format_type.cpp
)

if(WITH_WEB)
    list(APPEND LIB_SRC
        web.cpp
    )
    
    list(APPEND INCLUDE_DIRS ${LIBWEBSOCKETS_INCLUDE_DIRS})
    list(APPEND LIBRARIES PkgConfig::LIBWEBSOCKETS ${LIBWEBSOCKETS_LDFLAGS})
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

if(WITH_API)
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

add_library(villas SHARED ${LIB_SRC})

target_include_directories(villas PUBLIC ${INCLUDE_DIRS})
target_link_libraries(villas PUBLIC ${LIBRARIES} villas-common)

if(APPLE)
    target_link_libraries(villas PRIVATE -Wl,-all_load ${WHOLE_ARCHIVES} -Wl,-noall_load)
else()
    target_link_libraries(villas PRIVATE -Wl,--whole-archive ${WHOLE_ARCHIVES} -Wl,--no-whole-archive)
endif()

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

#install(
#    EXPORT VILLASNodeConfig
#   DESTINATION share/VILLASNode/cmake
#)

#export(
#    TARGETS villas
#    FILE VILLASNodeConfig.cmake
#)
