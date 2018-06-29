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

set(FORMAT_SRC
    json.c
    json_reserve.c
    villas_binary.c
    villas_human.c
    csv.c
    raw.c
    msg.c
)

set(INCLUDE_DIRS
    ${OPENSSL_INCLUDE_DIR}
)

set(LIBRARIES
    ${OPENSSL_LIBRARIES}
)

# Enable Google Protobuf format
if(ProtobufC_FOUND)
    list(APPEND FORMAT_SRC
        protobuf.c
        ${CMAKE_CURRENT_BINARY_DIR}/villas.pb-c.c
    )

    list(APPEND INCLUDE_DIRS
        ${PROTOBUFC_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}
    )

    list(APPEND LIBRARIES
        ${PROTOBUFC_LIBRARIES}
    )

    set_source_files_properties(villas.pb-c.h villas.pb-c.c
        PROPERTIES
            GENERATED TRUE
    )

    add_custom_command(
        COMMAND ${PROTOBUFC_COMPILER} --c_out=${CMAKE_CURRENT_BINARY_DIR} villas.proto
        OUTPUT
            villas.pb-c.c
            villas.pb-c.h
        MAIN_DEPENDENCY villas.proto
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

add_library(villas-formats SHARED ${FORMAT_SRC})
target_include_directories(villas-formats PUBLIC ${INCLUDE_DIRS})
target_link_libraries(villas-formats PUBLIC ${LIBRARIES})

#set_target_properties(villas-formats PROPERTIES
#    VERSION ${PROJECT_VERSION}
#    SOVERSION ${PROJECT_SOVERSION}
#)

install(
    TARGETS villas-formats
    COMPONENT lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
