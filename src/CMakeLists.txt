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

# All executables link against libvillas
link_libraries(villas)

add_executable(villas-node node.c)
add_executable(villas-test-rtt test-rtt.c)
add_executable(villas-test-shmem test-shmem.c)

set(SRCS )

if(WITH_IO)
    add_executable(villas-test-cmp test-cmp.c)
    
    add_executable(villas-convert convert.c)
    
    add_executable(villas-pipe pipe.c)
    target_link_libraries(villas-pipe PUBLIC pthread)

    add_executable(villas-signal signal.c)
endif()

if(WITH_IO AND WITH_HOOKS)
    add_executable(villas-hook hook.c)
endif()

install(
    TARGETS ${BUILDSYSTEM_TARGETS}
    COMPONENT bin
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
