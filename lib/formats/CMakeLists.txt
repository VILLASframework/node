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

if(DEFINED PROTOBUFC_COMPILER AND PROTOBUFC_FOUND)
    add_custom_command(
        OUTPUT
            villas.pb-c.c villas.pb-c.h
        COMMAND ${PROTOBUFC_COMPILER}
            --c_out=${CMAKE_CURRENT_BINARY_DIR}
            villas.proto
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
    raw.cpp
    value.cpp
    villas_binary.cpp
    villas_human.cpp
)

add_library(formats STATIC ${FORMAT_SRC})
target_include_directories(formats PUBLIC ${INCLUDE_DIRS})
target_link_libraries(formats PUBLIC ${LIBRARIES})
