# CMakeLists.
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

find_package(OpenSSL REQUIRED)
find_package(curl REQUIRED)

pkg_check_modules(JANSSON IMPORTED_TARGET REQUIRED jansson)

set(INCLUDE_DIRS
    ${JANSSON_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
)

set(LIBRARIES
    PkgConfig::JANSSON
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
)

set(LIB_SRC
    kernel/kernel.c
    kernel/rt.c
    sample.c
    path.c
    node.c
    log.c
    log_config.c
    utils.c
    super_node.c
    hist.c
    timing.c
    pool.c
    list.c
    queue.c
    queue_signalled.c
    memory.c
    advio.c
    plugin.c
    node_type.c
    stats.c
    mapping.c
    shmem.c
    config_helper.c
    crypt.c
    compat.c
    log_helper.c
    task.c
    buffer.c
    table.c
    bitset.c
    signal.c
)

add_subdirectory(nodes)

if(WITH_CONFIG)
    pkg_check_modules(CONFIG IMPORTED_TARGET REQUIRED libconfig)

    list(APPEND INCLUDE_DIRS ${CONFIG_INCLUDE_DIRS})
    list(APPEND LIBRARIES PkgConfig::CONFIG)
endif()

if(WITH_IO)
    list(APPEND LIB_SRC
        io.c
        format_type.c
    )
    
    add_subdirectory(formats)
    list(APPEND LIBRARIES villas-formats)
endif()

if(WITH_HOOKS)
    list(APPEND LIB_SRC
        hook.c
        hook_type.c
    )
    
    add_subdirectory(hooks)
    list(APPEND LIBRARIES villas-hooks)
endif()

if(WITH_WEB)
    list(APPEND LIB_SRC
        web.c
    )

    find_package(libwebsockets REQUIRED)

    message("LWS: ${LIBWEBSOCKETS_LIBRARIES}")
    
    list(APPEND INCLUDE_DIRS ${LIBWEBSOCKETS_INCLUDE_DIRS})
    list(APPEND LIBRARIES websockets_shared)
endif()

if(WITH_API AND WITH_WEB)
    list(APPEND LIB_SRC
        api.c
    )

    add_subdirectory(api)
    list(APPEND LIBRARIES villas-api)
endif()

add_library(villas SHARED ${LIB_SRC})

target_include_directories(villas PUBLIC ${INCLUDE_DIRS})
target_link_libraries(villas PUBLIC ${LIBRARIES})
