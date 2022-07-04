# CMakeLists.txt.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
###################################################################################

add_executable(villas-shmem villas-shmem.cpp)

target_link_libraries(villas-shmem PUBLIC villas)

install(
    TARGETS villas-shmem
    COMPONENT bin
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
