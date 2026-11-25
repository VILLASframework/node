# CMakeLists.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

add_compile_options(-fPIC)
add_compile_definitions(__STDC_FORMAT_MACROS _GNU_SOURCE)

list(APPEND INCLUDE_DIRS
    ${PROJECT_BINARY_DIR}/include
    ${PROJECT_SOURCE_DIR}/include
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
)

set(LIBRARIES
    nlohmann_json::nlohmann_json
    PkgConfig::JANSSON
    PkgConfig::UUID
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    ${CMAKE_DL_LIBS}
    spdlog::spdlog
    fmt::fmt
    m
    stdc++
    rt
    nlohmann_json_schema_validator
)

set(LIB_SRC
    base64.cpp
    buffer.cpp
    capabilities.cpp
    common.cpp
    compat.cpp
    config_helper.cpp
    cpuset.cpp
    dsp/pid.cpp
    dumper.cpp
    format.cpp
    hist.cpp
    json.cpp
    kernel/kernel.cpp
    kernel/rt.cpp
    list.cpp
    log.cpp
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
    json_schema.cpp
    path_destination.cpp
    path_source.cpp
    path.cpp
    path_list.cpp
    plugin.cpp
    pool.cpp
    popen.cpp
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
    table.cpp
    task.cpp
    terminal.cpp
    timing.cpp
    tool.cpp
    utils.cpp
    uuid.cpp
    version.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    list(APPEND LIB_SRC tsc.cpp)
endif()

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
    list(APPEND WHOLE_ARCHIVES villas-fpga)
    list(APPEND INCLUDE_DIRS $<TARGET_PROPERTY:villas-fpga,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

add_library(villas SHARED ${LIB_SRC})
target_include_directories(villas PUBLIC ${INCLUDE_DIRS})
target_link_libraries(villas PUBLIC ${LIBRARIES})
target_compile_definitions(villas PUBLIC __STDC_FORMAT_MACROS _GNU_SOURCE)
target_link_libraries(villas PRIVATE -Wl,--whole-archive ${WHOLE_ARCHIVES} -Wl,--no-whole-archive)

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
