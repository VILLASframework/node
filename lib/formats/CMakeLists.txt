# CMakeLists.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

if(DEFINED PROTOBUF_COMPILER AND PROTOBUF_FOUND)
    add_custom_command(
        OUTPUT
            villas.pb.cc villas.pb.h
            message.js sample.js timestamp.js value.js
        COMMAND ${PROTOBUF_COMPILER}
            --js_out=${CMAKE_CURRENT_BINARY_DIR}
            --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
            villas.proto
        MAIN_DEPENDENCY villas.proto
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

if(DEFINED PROTOBUF_COMPILER AND DEFINED PROTOBUFC_COMPILER AND PROTOBUFC_FOUND)
    add_custom_command(
        OUTPUT
            villas.pb-c.c
            villas.pb-c.h
        DEPENDS
            villas.proto
        COMMAND ${PROTOBUF_COMPILER}
            --c_out=${CMAKE_CURRENT_BINARY_DIR}
            villas.proto
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/villas.pb-c.h
            ${CMAKE_BINARY_DIR}/include/villas/formats/villas.pb-c.h
        MAIN_DEPENDENCY villas.proto
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    list(APPEND FORMAT_SRC
        protobuf.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/villas.pb-c.c
    )

    list(APPEND INCLUDE_DIRS
        ${CMAKE_CURRENT_BINARY_DIR}
    )

    list(APPEND LIBRARIES
        PkgConfig::PROTOBUFC
    )
endif()

list(APPEND FORMAT_SRC
    column.cpp
    iotagent_ul.cpp
    json_edgeflex.cpp
    json_kafka.cpp
    json_reserve.cpp
    json.cpp
    line.cpp
    msg.cpp
    opal_asyncip.cpp
    raw.cpp
    value.cpp
    villas_binary.cpp
    villas_human.cpp
)

add_library(formats STATIC ${FORMAT_SRC})
target_include_directories(formats PUBLIC ${INCLUDE_DIRS})
target_link_libraries(formats PUBLIC ${LIBRARIES})
