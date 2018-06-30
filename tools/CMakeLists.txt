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

if(LIBCONFIG_FOUND)
    add_executable(conf2json conf2json.c)
    target_link_libraries(conf2json PUBLIC villas)

    list(APPEND TOOLS conf2json)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(rmshm rmshm.c)
    target_link_libraries(rmshm PUBLIC Threads::Threads rt)

    add_executable(rmsem rmsem.c)
    target_link_libraries(rmsem PUBLIC Threads::Threads rt)

    list(APPEND TOOLS rmsem rmshm)
endif()

if(LIBZMQ_FOUND)
    add_executable(zmq-keygen zmq-keygen.c)
    target_include_directories(zmq-keygen PUBLIC ${LIBZMQ_INCLUDE_DIRS})
    target_link_libraries(zmq-keygen PUBLIC PkgConfig::LIBZMQ)
    
    list(APPEND TOOLS zmq-keygen)
endif()

install(
    TARGETS ${TOOLS}
    COMPONENT tools
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    PROGRAMS villas.sh
    COMPONENT bin
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    RENAME villas
)
