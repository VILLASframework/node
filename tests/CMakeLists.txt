# Makefile.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

add_custom_target(tests)
add_custom_target(run-tests)

add_subdirectory(integration)
if(CRITERION_FOUND)
	add_subdirectory(unit)
endif()

if(WITH_SRC AND WITH_HOOKS)
	set(VALGRIND "valgrind --leak-check=full --show-leak-kinds=all --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp")

	add_custom_target(run-valgrind
		COMMAND ${VALGRIND} $<TARGET_FILE:villas-node> & sleep 2; kill %1
		COMMAND ${VALGRIND} $<TARGET_FILE:villas-pipe> -t 2 ${PROJECT_SOURCE_DIR}/etc/websocket-loopback.conf ws1
		COMMAND ${VALGRIND} $<TARGET_FILE:villas-signal> mixed -v 4 -l 10
		COMMAND ${VALGRIND} $<TARGET_FILE:villas-hook> stats < <($<TARGET_FILE:villas-signal> mixed -l 5)
	)
	add_dependencies(run-valgrind villas-node villas-pipe villas-signal villas-hook)
endif()
