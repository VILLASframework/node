# Makefile.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

if(DEFINED PROTOBUF_COMPILER AND PROTOBUF_FOUND)
    add_custom_command(
        OUTPUT
            villas_pb2.py
        COMMAND ${PROTOBUF_COMPILER}
            --python_out=${CMAKE_CURRENT_SOURCE_DIR}/villas/node
            villas.proto
        MAIN_DEPENDENCY ${PROJECT_SOURCE_DIR}/lib/formats/villas.proto
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/lib/formats
    )

    add_custom_target(python ALL
        DEPENDS
            villas_pb2.py
    )
endif()
