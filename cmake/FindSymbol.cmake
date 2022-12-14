# CMakeLists.txt.
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
###################################################################################

function(FindSymbol LIBRARY SYMBOL FOUND)
	find_program(OBJDUMP_EXECUTABLE NAMES objdump)

	execute_process(
		COMMAND /bin/sh -c "${OBJDUMP_EXECUTABLE} -T ${LIBRARY} | grep ${SYMBOL} | wc -l"
		OUTPUT_VARIABLE OUTPUT
	)

	if(OUTPUT GREATER 0)
		set(${FOUND} "${SYMBOL}" PARENT_SCOPE)
	else()
		set(${FOUND} "${SYMBOL}-NOTFOUND" PARENT_SCOPE)
	endif()
endfunction()
