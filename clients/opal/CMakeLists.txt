# CMakeLists.txt.
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
###################################################################################

if(OPAL_FOUND)
    # Just call the original Makefile
    add_custom_target(clients-opal
        COMMAND make -C ${CMAKE_CURRENT_SOURCE_DIR}/models/send_receive
                     -f Makefile.mk AsyncIP
                     RTLAB_INTEL_COMPILER=0
                     PROTOCOL=VILLAS
                     OPAL_LIBS="-lSystem -luuid"
                     OPAL_LIBPATH=-L${OPAL_LIBRARY_DIR}
                     OPAL_INCPATH=-I${OPAL_INCLUDE_DIR}
    )
endif()
