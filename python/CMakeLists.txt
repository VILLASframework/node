# Makefile.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
###################################################################################

if(DEFINED PROTOBUF_COMPILER AND PROTOBUF_FOUND)
    add_custom_command(
        OUTPUT
            villas_pb2.py
        COMMAND ${PROTOBUF_COMPILER}
            --python_out=${CMAKE_CURRENT_BINARY_DIR}
            villas.proto
        MAIN_DEPENDENCY ${PROJECT_SOURCE_DIR}/lib/formats/villas.proto
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/lib/formats
    )

    add_custom_target(python ALL
        DEPENDS
            villas_pb2.py
    )
endif()
