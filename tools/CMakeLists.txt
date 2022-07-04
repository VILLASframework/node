# CMakeLists.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
###################################################################################

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    add_executable(rmshm rmshm.cpp)
    target_link_libraries(rmshm PUBLIC Threads::Threads rt)

    add_executable(rmsem rmsem.cpp)
    target_link_libraries(rmsem PUBLIC Threads::Threads rt)

    list(APPEND TOOLS rmsem rmshm)
endif()

install(
    TARGETS ${TOOLS}
    COMPONENT tools
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    PROGRAMS villas
    COMPONENT bin
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    RENAME villas
)

install(
    PROGRAMS villas-api.sh
    COMPONENT bin
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    RENAME villas-api
)
