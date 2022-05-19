## CMakeLists.txt
#
# @author Daniel Krebs <github@daniel-krebs.net>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache License 2.0
#
# VILLAScommon
#
##############################################################################

add_executable(unit-tests-common
	buffer.cpp
	graph.cpp
	hist.cpp
	kernel.cpp
	list.cpp
	task.cpp
	timing.cpp
	utils.cpp
	base64.cpp
	popen.cpp
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	list(APPEND TEST_SRC tsc.cpp)
endif()

target_include_directories(unit-tests-common PUBLIC
	${PROJECT_SOURCE_DIR}/include
	${CRITERION_INCLUDE_DIRS}
)

target_link_libraries(unit-tests-common PUBLIC
	villas-common
	${CRITERION_LIBRARIES}
)

add_custom_target(run-unit-tests-common
	COMMAND $<TARGET_FILE:unit-tests-common> ${CRITERION_OPTS}
	USES_TERMINAL
)
