# CMakeLists.
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

add_compile_options(-fPIC)

list(APPEND INCLUDE_DIRS
    ${JANSSON_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}/include
    ${PROJECT_SOURCE_DIR}/include
)

set(LIBRARIES
    PkgConfig::JANSSON
    villas-common
    m
    stdc++
)

if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
	list(APPEND LIBRARIES "-lstdc++fs")
endif()

set(LIB_SRC
    super_node.cpp
    memory/heap.c
    memory/hugepage.c
    memory/managed.c
    sample.c
    path.c
    node.c
    memory.c
    plugin.c
    node_type.c
    stats.c
    mapping.c
    shmem.c
    config_helper.c
    signal.c
    copyright.c
    pool.c
    queue.c
    queue_signalled.c
)

if(IBVERBS_FOUND AND RDMACM_FOUND)
    list(APPEND LIB_SRC memory/ib.c)
endif()

add_subdirectory(nodes)
list(APPEND WHOLE_ARCHIVES nodes)

if(LIBCONFIG_FOUND)
    list(APPEND INCLUDE_DIRS ${LIBCONFIG_INCLUDE_DIRS})
    list(APPEND LIBRARIES PkgConfig::LIBCONFIG)
endif()

if(WITH_IO)
    list(APPEND LIB_SRC
        io.c
        format_type.c
    )
    
    add_subdirectory(formats)
    list(APPEND WHOLE_ARCHIVES formats)
endif()

if(WITH_HOOKS)
    list(APPEND LIB_SRC
        hook.c
        hook_type.c
    )

    add_subdirectory(hooks)
    list(APPEND WHOLE_ARCHIVES hooks)
endif()

if(WITH_WEB)
    list(APPEND LIB_SRC
        web.cpp
    )
    
    list(APPEND INCLUDE_DIRS ${LIBWEBSOCKETS_INCLUDE_DIRS})
    list(APPEND LIBRARIES PkgConfig::LIBWEBSOCKETS)
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
        kernel/nl.c
        kernel/tc.c
        kernel/tc_netem.c
        kernel/if.c
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
)

install(
    DIRECTORY ${PROJECT_BINARY_DIR}/include/villas/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/villas
    COMPONENT devel
    FILES_MATCHING
        PATTERN "*.h"
)

#install(
#    EXPORT VILLASNodeConfig
#   DESTINATION share/VILLASNode/cmake
#)

#export(
#    TARGETS villas
#    FILE VILLASNodeConfig.cmake
#)
