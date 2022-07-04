# Makefile.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
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
